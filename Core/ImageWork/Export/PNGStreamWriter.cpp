#include "StdAfx.h"
#include "PngStreamWriter.h"

#include <QtZlib/zlib.h>

#include "Core/FileSystem/LFSFacade.h"

namespace
{
	namespace JDKF = JDKLevelMaps::ImageWork;

	inline constexpr void WriteBigEndian32(uint8* buffer, uint32 value) noexcept;
	inline bool WriteChunk(FILE* pFile, const char* chunkType, const uint8* pData, uint32 length) noexcept;

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

		bool Init()
		{
			if (deflateInit(&stream, Z_BEST_COMPRESSION) != Z_OK)
				return false;

			stream.next_out = outBuffer.data();
			stream.avail_out = static_cast<uInt>(outBuffer.size());
			return true;
		}

		bool Flush(bool bFinish)
		{
			while (stream.avail_out == 0 || bFinish)
			{
				const size_t bytesReady = outBuffer.size() - stream.avail_out;
				if (bytesReady > 0)
				{
					if (!WriteChunk(pFile, "IDAT", outBuffer.data(), static_cast<uint32>(bytesReady)))
						return false;

					stream.next_out = outBuffer.data();
					stream.avail_out = static_cast<uInt>(outBuffer.size());
				}

				if (!bFinish) break;

				int ret = deflate(&stream, Z_FINISH);
				if (ret == Z_STREAM_END)
				{
					const size_t finalBytes = outBuffer.size() - stream.avail_out;

					if (finalBytes > 0 && !WriteChunk(pFile, "IDAT", outBuffer.data(), static_cast<uint32>(finalBytes)))
						return false;
					break;
				}
				if (ret != Z_OK) return false;
			}
			return true;
		}
	};

	inline constexpr void WriteBigEndian32(uint8* buffer, uint32 value) noexcept
	{
		buffer[0] = static_cast<uint8>(value >> 24);
		buffer[1] = static_cast<uint8>(value >> 16);
		buffer[2] = static_cast<uint8>(value >> 8);
		buffer[3] = static_cast<uint8>(value);
	}

	inline bool WriteChunk(FILE* pFile, const char* chunkType, const uint8* pData, uint32 length) noexcept
	{
		uint8 lenBuf[4];
		WriteBigEndian32(lenBuf, length);
		if (gEnv->pCryPak->FWrite(lenBuf, 1, 4, pFile) != 4)
			return false;
		if (gEnv->pCryPak->FWrite(chunkType, 1, 4, pFile) != 4)
			return false;

		uint32 crc = crc32(0L, reinterpret_cast<const Bytef*>(chunkType), 4);
		if (length > 0 && pData)
		{
			if (gEnv->pCryPak->FWrite(pData, 1, length, pFile) != length)
				return false;

			crc = crc32(crc, reinterpret_cast<const Bytef*>(pData), length);
		}

		uint8 crcBuf[4];
		WriteBigEndian32(crcBuf, crc);
		return gEnv->pCryPak->FWrite(crcBuf, 1, 4, pFile) == 4;
	}

	inline bool WriteSignature(FILE* pFile) noexcept
	{
		static const uint8 pngSignature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
		return gEnv->pCryPak->FWrite(pngSignature, 1, 8, pFile) == 8;
	}

	inline bool WriteIHDR(FILE* pFile, uint32 width, uint32 height) noexcept
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

	inline bool WriteIEND(FILE* pFile) noexcept
	{
		return WriteChunk(pFile, "IEND", nullptr, 0);
	}

	inline bool StreamIDAT(FILE* pFile, uint32 width, uint32 height, JDKF::IPNGDataSource& dataSource)
	{
		SIDATCompressor compressor(pFile);
		if (!compressor.Init())
			return false;

		std::vector<uint8> rowBuffer(1 + static_cast<size_t>(width) * 3);
		rowBuffer[0] = 0x00; // None filter

		for (uint32 y = 0; y < height; ++y)
		{
			if (!dataSource.OnProgress(y) || !dataSource.FetchRowRGB(y, rowBuffer.data() + 1))
				return false;

			compressor.stream.next_in = rowBuffer.data();
			compressor.stream.avail_in = static_cast<uInt>(rowBuffer.size());

			while (compressor.stream.avail_in > 0)
				if (deflate(&compressor.stream, Z_NO_FLUSH) != Z_OK || !compressor.Flush(false))
					return false;
		}

		return compressor.Flush(true);
	}
}

namespace JDKLevelMaps::ImageWork
{
	bool WritePNG(FILE* pFile, uint32 width, uint32 height, IPNGDataSource& dataSource)
	{
		if (!pFile || width == 0 || height == 0)
			return false;

		if (width > (std::numeric_limits<uInt>::max() - 1) / 3)
			return false;

		if (!WriteSignature(pFile))
			return false;

		if (!WriteIHDR(pFile, width, height))
			return false;

		if (!StreamIDAT(pFile, width, height, dataSource))
			return false;

		if (!WriteIEND(pFile))
			return false;

		if (gEnv->pCryPak->FFlush(pFile) != 0)
			return false;

		dataSource.OnProgress(height);
		return true;
	}
}