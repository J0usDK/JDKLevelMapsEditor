#include "StdAfx.h"
#include "PNGStreamReader.h"

#include <QtZlib/zlib.h>

#include "Core/Data/RunResult.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/ImageView.h"
#include "Utils/Progress.h"

namespace
{
	namespace JDKM = JDKLevelMaps::ImageWork;

	struct SIDATDecompressor
	{
		static constexpr size_t kInputBufferSize = 64 * 1024;

		FILE* pFile = nullptr;
		z_stream stream{};

		std::vector<uint8> inBuffer;
		std::vector<uint8> rowBuffer;

		uint32 currentRow = 0;
		uint32 currentRowBytes = 0;
		const uint32 targetRowBytes;

		const JDKM::SImageSizes imageSizes;
		JDKM::SImageView& outImageView;
		JDKLevelMaps::Utils::Common::SProgressTask* pTask = nullptr;

		SIDATDecompressor(FILE* pFile, const JDKM::SImageSizes sizes, JDKM::SImageView& imageView, JDKLevelMaps::Utils::Common::SProgressTask* pTask)
			: pFile(pFile), targetRowBytes(sizes.width * 3 + 1), imageSizes(sizes), outImageView(imageView), pTask(pTask)
		{
			inBuffer.resize(kInputBufferSize);
			rowBuffer.resize(targetRowBytes);
		}
		~SIDATDecompressor()
		{
			inflateEnd(&stream);
		}

		bool Init()
		{
			return inflateInit(&stream) == Z_OK;
		}

		bool ProcessChunk(uint32 chunkLength)
		{
			uint32 remaining = chunkLength;
			while (remaining > 0)
			{
				const uint32 toRead = std::min(remaining, static_cast<uint32>(inBuffer.size()));
				if (gEnv->pCryPak->FReadRaw(inBuffer.data(), 1, toRead, pFile) != toRead)
					return false;
				remaining -= toRead;

				stream.next_in = inBuffer.data();
				stream.avail_in = toRead;

				while (stream.avail_in > 0)
				{
					stream.next_out = rowBuffer.data() + currentRowBytes;
					stream.avail_out = targetRowBytes - currentRowBytes;

					const int ret = inflate(&stream, Z_NO_FLUSH);
					if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
						return false;
					currentRowBytes = targetRowBytes - stream.avail_out;

					if (currentRowBytes == targetRowBytes)
						if (!FlushRow())
							return false;
				}
			}
			return JDKLevelMaps::FileSystem::LFSFacade::FSeek(pFile, 4, SEEK_CUR) == 0;
		}

	private:
		bool FlushRow()
		{
			if (currentRow < imageSizes.height)
			{
				uint8* pDestRow = outImageView.ScanLine(currentRow);
				if (rowBuffer[0] != 0)
					return false;
				std::memcpy(pDestRow, rowBuffer.data() + 1, static_cast<size_t>(imageSizes.width) * 3);

				if (pTask && !pTask->Update(currentRow + 1))
					return false;
				currentRow++;
			}
			currentRowBytes = 0;
			return true;
		}
	};

	inline uint32 ReadBigEndian32(const uint8* buffer) noexcept
	{
		return (static_cast<uint32>(buffer[0]) << 24) |
			   (static_cast<uint32>(buffer[1]) << 16) |
			   (static_cast<uint32>(buffer[2]) << 8)  |
			   (static_cast<uint32>(buffer[3]));
	}

	inline constexpr uint32 MakeFourCC(char a, char b, char c, char d) noexcept
	{
		return (static_cast<uint32>(a) << 24) |
			(static_cast<uint32>(b) << 16) |
			(static_cast<uint32>(c) << 8) |
			(static_cast<uint32>(d));
	}

	inline bool VerifySignature(FILE* pFile) noexcept
	{
		static constexpr uint8 expectedSignature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
		uint8 signature[8];
		if (gEnv->pCryPak->FReadRaw(signature, 1, 8, pFile) != 8)
			return false;
		return std::memcmp(signature, expectedSignature, 8) == 0;
	}

	inline bool ReadChunkHeader(FILE* pFile, uint32& outLength, uint32& outType) noexcept
	{
		uint8 header[8];
		if (gEnv->pCryPak->FReadRaw(header, 1, 8, pFile) != 8)
			return false;

		outLength = ReadBigEndian32(header);
		outType = ReadBigEndian32(header + 4);
		return true;
	}
}

namespace JDKLevelMaps::ImageWork
{
	Data::SRunResult ReadPNGInfo(FILE* pFile, SImageSizes& outImageSizes)
	{
		if (!pFile)
			return { false, "File pointer is null" };

		if (!VerifySignature(pFile))
			return { false, "Invalid PNG signature" };

		uint32 length;
		uint32 type;
		if (!ReadChunkHeader(pFile, length, type) || type != MakeFourCC('I', 'H', 'D', 'R') || length != 13)
			return { false, "Failed to read IHDR chunk or invalid length" };

		uint8 ihdrData[13];
		if (gEnv->pCryPak->FReadRaw(ihdrData, 1, 13, pFile) != 13)
			return { false, "Failed to read IHDR data" };

		if (FileSystem::LFSFacade::FSeek(pFile, 4, SEEK_CUR) != 0)
			return { false, "Failed to skip IHDR CRC" };

		outImageSizes.width = ReadBigEndian32(&ihdrData[0]);
		outImageSizes.height = ReadBigEndian32(&ihdrData[4]);

		if (ihdrData[8] != 8 || ihdrData[9] != 2 || ihdrData[10] != 0 || ihdrData[11] != 0 || ihdrData[12] != 0)
			return { false, "Unsupported PNG format" };

		return { true, "" };
	}

	Data::SRunResult ReadPNG(FILE* pFile, SImageSizes& imageSizes, SImageView& outImageView, Utils::Common::SProgressTask* pTask)
	{
		if (!pFile)
			return { false, "File pointer is null" };

		static constexpr uint64 kPNGDataOffset = 33;
		if (FileSystem::LFSFacade::FSeek(pFile, kPNGDataOffset, SEEK_SET) != 0)
			return { false, "Disk I/O Error: Cannot seek to PNG data chunks" };

		SIDATDecompressor decompressor(pFile, imageSizes, outImageView, pTask);
		if (!decompressor.Init())
			return { false, "Failed to initialize zlib stream" };

		while (true)
		{
			uint32 length;
			uint32 type;

			if (!ReadChunkHeader(pFile, length, type))
				return { false, "Failed to read chunk header during decode" };

			if (type == MakeFourCC('I', 'D', 'A', 'T'))
			{
				if (!decompressor.ProcessChunk(length))
					return { false, "Failed to decompress IDAT chunk" };
			}
			else if (type == MakeFourCC('I', 'E', 'N', 'D'))
				break;
			else if (FileSystem::LFSFacade::FSeek(pFile, length + 4, SEEK_CUR) != 0)
				return { false, "Failed to skip unknown chunk" };
		}

		if (decompressor.currentRow != imageSizes.height)
			return { false, "Image decoded incompletely (missing rows)" };
		return { true, "" };
	}
}