#include "StdAfx.h"
#include "PngStreamWriter.h"

#include <QtZlib/zlib.h>

#include "Core/Data/RunResult.h"
#include "Core/FileSystem/LFSFacade.h"

namespace
{
	namespace JDKF = JDKLevelMaps::ImageWork;

	inline constexpr void WriteBigEndian32(uint8* buffer, uint32 value) noexcept;
	inline JDKLevelMaps::Data::SRunResult WriteChunk(FILE* pFile, const char* chunkType, const uint8* pData, uint32 length) noexcept;

	struct SIDATCompressor
	{
		const size_t kOutputBufferSize = 64 * 1024;

		FILE* pFile = nullptr;
		z_stream stream{};
		std::vector<uint8> outBuffer;

		SIDATCompressor(FILE* file) : pFile(file)
		{
			outBuffer.resize(kOutputBufferSize);
		}
		~SIDATCompressor() { deflateEnd(&stream); }

		JDKLevelMaps::Data::SRunResult Init()
		{
			if (deflateInit(&stream, Z_BEST_COMPRESSION) != Z_OK)
				return { false, "ZLib Error: Failed to initialize deflate stream" };

			stream.next_out = outBuffer.data();
			stream.avail_out = static_cast<uInt>(outBuffer.size());
			return { true, "" };
		}

		JDKLevelMaps::Data::SRunResult Flush(bool bFinish)
		{
			while (stream.avail_out == 0 || bFinish)
			{
				const size_t bytesReady = outBuffer.size() - stream.avail_out;
				if (bytesReady > 0)
				{
					if (auto result = WriteChunk(pFile, "IDAT", outBuffer.data(), static_cast<uint32>(bytesReady)); !result.bSuccess)
						return result;

					stream.next_out = outBuffer.data();
					stream.avail_out = static_cast<uInt>(outBuffer.size());
				}

				if (!bFinish) break;

				int ret = deflate(&stream, Z_FINISH);
				if (ret == Z_STREAM_END)
				{
					const size_t finalBytes = outBuffer.size() - stream.avail_out;
					if (finalBytes > 0)
						if (auto result = WriteChunk(pFile, "IDAT", outBuffer.data(), static_cast<uint32>(finalBytes)); !result.bSuccess)
							return result;
					break;
				}
				if (ret != Z_OK)
					return { false, "ZLib Error: Deflate compression failed" };
			}
			return { true, "" };
		}
	};

	inline constexpr void WriteBigEndian32(uint8* buffer, uint32 value) noexcept
	{
		buffer[0] = static_cast<uint8>(value >> 24);
		buffer[1] = static_cast<uint8>(value >> 16);
		buffer[2] = static_cast<uint8>(value >> 8);
		buffer[3] = static_cast<uint8>(value);
	}

	inline JDKLevelMaps::Data::SRunResult WriteChunk(FILE* pFile, const char* chunkType, const uint8* pData, uint32 length) noexcept
	{
		uint8 lenBuf[4];
		WriteBigEndian32(lenBuf, length);
		if (gEnv->pCryPak->FWrite(lenBuf, 1, 4, pFile) != 4)
			return { false, std::string("Disk I/O Error: Failed to write ") + chunkType + " chunk length" };
		if (gEnv->pCryPak->FWrite(chunkType, 1, 4, pFile) != 4)
			return { false, std::string("Disk I/O Error: Failed to write ") + chunkType + " chunk type" };

		uint32 crc = crc32(0L, reinterpret_cast<const Bytef*>(chunkType), 4);
		if (length > 0 && pData)
		{
			if (gEnv->pCryPak->FWrite(pData, 1, length, pFile) != length)
				return { false, std::string("Disk I/O Error: Failed to write ") + chunkType + " chunk data" };

			crc = crc32(crc, reinterpret_cast<const Bytef*>(pData), length);
		}

		uint8 crcBuf[4];
		WriteBigEndian32(crcBuf, crc);

		if (gEnv->pCryPak->FWrite(crcBuf, 1, 4, pFile) != 4)
			return { false, std::string("Disk I/O Error: Failed to write ") + chunkType + " chunk CRC" };

		return { true, "" };
	}

	inline JDKLevelMaps::Data::SRunResult WriteSignature(FILE* pFile) noexcept
	{
		static const uint8 pngSignature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

		if (gEnv->pCryPak->FWrite(pngSignature, 1, 8, pFile) != 8)
			return { false, "Disk I/O Error: Failed to write PNG signature" };
		return { true, "" };
	}

	inline JDKLevelMaps::Data::SRunResult WriteIHDR(FILE* pFile, uint32 width, uint32 height) noexcept
	{
		uint8 ihdrData[13];
		WriteBigEndian32(&ihdrData[0], width);
		WriteBigEndian32(&ihdrData[4], height);
		ihdrData[8] = 8; // Bit depth
		ihdrData[9] = 2; // Color type: RGB
		ihdrData[10] = 0; // Compression: Deflate
		ihdrData[11] = 0; // Filter
		ihdrData[12] = 0; // Interlace

		return WriteChunk(pFile, "IHDR", ihdrData, 13);
	}

	inline JDKLevelMaps::Data::SRunResult WriteIEND(FILE* pFile) noexcept
	{
		return WriteChunk(pFile, "IEND", nullptr, 0);
	}

	inline JDKLevelMaps::Data::SRunResult StreamIDAT(FILE* pFile, uint32 width, uint32 height, JDKF::IPNGDataSource& dataSource)
	{
		SIDATCompressor compressor(pFile);
		if (auto result = compressor.Init(); !result.bSuccess)
			return result;

		std::vector<uint8> rowBuffer(1 + static_cast<size_t>(width) * 3);
		rowBuffer[0] = 0x00; // None filter

		for (uint32 y = 0; y < height; ++y)
		{
			if (!dataSource.OnProgress(y))
				return { false, "Operation cancelled during PNG export" };

			if (!dataSource.FetchRowRGB(y, rowBuffer.data() + 1))
				return { false, "Failed to fetch image row data from baker" };

			compressor.stream.next_in = rowBuffer.data();
			compressor.stream.avail_in = static_cast<uInt>(rowBuffer.size());

			while (compressor.stream.avail_in > 0)
			{
				if (deflate(&compressor.stream, Z_NO_FLUSH) != Z_OK)
					return { false, "ZLib Error: Deflate process failed on row " + std::to_string(y) };

				if (auto result = compressor.Flush(false); !result.bSuccess)
					return result;
			}
		}

		return compressor.Flush(true);
	}
}

namespace JDKLevelMaps::ImageWork
{
	Data::SRunResult WritePNG(FILE* pFile, uint32 width, uint32 height, IPNGDataSource& dataSource)
	{
		if (!pFile)
			return { false, "Internal Error: File pointer is null" };

		if (width == 0 || height == 0)
			return { false, "Invalid image dimensions for PNG export" };

		if (width > (std::numeric_limits<uInt>::max() - 1) / 3)
			return { false, "Image width is too large for PNG export" };

		if (auto result = WriteSignature(pFile); !result.bSuccess)
			return result;

		if (auto result = WriteIHDR(pFile, width, height); !result.bSuccess)
			return result;

		if (auto result = StreamIDAT(pFile, width, height, dataSource); !result.bSuccess)
			return result;

		if (auto result = WriteIEND(pFile); !result.bSuccess)
			return result;

		if (gEnv->pCryPak->FFlush(pFile) != 0)
			return { false, "Disk I/O Error: Failed to flush PNG file to disk" };

		dataSource.OnProgress(height);
		return { true, "" };
	}
}