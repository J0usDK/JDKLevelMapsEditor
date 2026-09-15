#include "StdAfx.h"
#include "LZ4CompressionStrategy.h"

#include <Includes/LZ4/lz4.h>
#include <Includes/LZ4/lz4frame.h>

#include "Core/Data/RunResult.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	struct SLZ4CompressionContext final : public ICompressionContext
	{
		LZ4F_cctx* cctx = nullptr;
		std::vector<uint8> internalBuf;

		size_t pendingHeaderSize = 0;
		bool headerWritten = false;

		SLZ4CompressionContext()
		{
			if (LZ4F_isError(LZ4F_createCompressionContext(&cctx, LZ4F_VERSION)))
				cctx = nullptr;

			const size_t bound = LZ4F_compressBound(1024 * 1024, nullptr) + LZ4F_HEADER_SIZE_MAX + LZ4F_BLOCK_CHECKSUM_SIZE;
			Utils::Common::TryResize(internalBuf, bound);
		}

		~SLZ4CompressionContext() override
		{
			if (cctx)
				LZ4F_freeCompressionContext(cctx);
		}
	};

	struct SLZ4DecompressionContext final : public IDecompressionContext
	{
		LZ4F_dctx* dctx = nullptr;
		std::vector<uint8> internalBuf;

		SLZ4DecompressionContext()
		{
			LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
			Utils::Common::TryResize(internalBuf, 1024 * 1024);
		}

		~SLZ4DecompressionContext() override
		{
			if (dctx)
				LZ4F_freeDecompressionContext(dctx);
		}
	};

	size_t CLZ4CompressionStrategy::GetMaxCompressedSize(size_t rawSize) const noexcept
	{
		if (rawSize > static_cast<size_t>(LZ4_MAX_INPUT_SIZE))
			return 0;

		return static_cast<size_t>(LZ4_compressBound(static_cast<int>(rawSize)));
	}

	Data::SRunResult CLZ4CompressionStrategy::Compress(const void* pSrc, size_t srcSize, void* pDst, size_t dstCapacity, size_t& outCompressedSize) const noexcept
	{
		const int result = LZ4_compress_default(static_cast<const char*>(pSrc), static_cast<char*>(pDst), static_cast<int>(srcSize), static_cast<int>(dstCapacity));
		if (result <= 0)
			return { false, "LZ4 Compression failed: Output buffer too small or input corrupted" };

		outCompressedSize = static_cast<size_t>(result);
		return { true, "" };
	}

	Data::SRunResult CLZ4CompressionStrategy::Decompress(const void* pSrc, size_t compressedSize, void* pDst, size_t rawSize) const noexcept
	{
		const int result = LZ4_decompress_safe(static_cast<const char*>(pSrc), static_cast<char*>(pDst), static_cast<int>(compressedSize), static_cast<int>(rawSize));

		if (result < 0)
			return { false, "LZ4 Decompression failed: Data is corrupted" };
		if (static_cast<size_t>(result) != rawSize)
			return { false, "LZ4 Decompression failed: Size mismatch" };

		return { true, "" };
	}

	std::unique_ptr<ICompressionContext> CLZ4CompressionStrategy::BeginCompression() const noexcept
	{
		auto pCtx = std::make_unique<SLZ4CompressionContext>();
		if (!pCtx->cctx || pCtx->internalBuf.empty())
			return nullptr;

		const size_t headerSize = LZ4F_compressBegin(pCtx->cctx, pCtx->internalBuf.data(), pCtx->internalBuf.size(), nullptr);
		if (LZ4F_isError(headerSize))
			return nullptr;

		pCtx->pendingHeaderSize = headerSize;

		return pCtx;
	}

	Data::SRunResult CLZ4CompressionStrategy::CompressChunk(ICompressionContext& ctx, const void* pSrc, size_t srcSize, std::vector<uint8>& outCompressed) const noexcept
	{
		auto* pZctx = static_cast<SLZ4CompressionContext*>(&ctx);
		if (!pZctx)
			return { false, "Internal Error: Failed to cast LZ4 compression context" };

		if (srcSize == 0)
			return { true, "" };

		if (!pZctx->headerWritten)
		{
			const size_t oldSize = outCompressed.size();
			if (!Utils::Common::TryResize(outCompressed, oldSize + pZctx->pendingHeaderSize))
				return { false, "Out of Memory: Failed to append LZ4 frame header" };

			std::memcpy(outCompressed.data() + oldSize, pZctx->internalBuf.data(), pZctx->pendingHeaderSize);
			pZctx->headerWritten = true;
		}

		size_t processedBytes = 0;
		const uint8* pRawData = static_cast<const uint8*>(pSrc);

		while (processedBytes < srcSize)
		{
			const size_t bytesToProcess = std::min(srcSize - processedBytes, static_cast<size_t>(1024 * 1024));
			const size_t compSize = LZ4F_compressUpdate(pZctx->cctx, pZctx->internalBuf.data(), pZctx->internalBuf.size(), pRawData + processedBytes, bytesToProcess, nullptr);

			if (LZ4F_isError(compSize))
				return { false, std::string("LZ4 stream compression failed: ") + LZ4F_getErrorName(compSize) };

			if (compSize > 0)
			{
				const size_t oldSize = outCompressed.size();
				if (!Utils::Common::TryResize(outCompressed, oldSize + compSize))
					return { false, "Out of Memory: Failed to append compressed data" };

				std::memcpy(outCompressed.data() + oldSize, pZctx->internalBuf.data(), compSize);
			}

			processedBytes += bytesToProcess;
		}

		return { true, "" };
	}

	Data::SRunResult CLZ4CompressionStrategy::EndCompression(ICompressionContext& ctx, std::vector<uint8>& outCompressed) const noexcept
	{
		auto* pZctx = static_cast<SLZ4CompressionContext*>(&ctx);
		if (!pZctx)
			return { false, "Internal Error: Failed to cast LZ4 compression context" };

		const size_t endSize = LZ4F_compressEnd(pZctx->cctx, pZctx->internalBuf.data(), pZctx->internalBuf.size(), nullptr);
		if (LZ4F_isError(endSize))
			return { false, std::string("LZ4 Stream end write failed: ") + LZ4F_getErrorName(endSize) };

		if (endSize > 0)
		{
			const size_t oldSize = outCompressed.size();
			if (!Utils::Common::TryResize(outCompressed, oldSize + endSize))
				return { false, "Out of Memory: Failed to append final compressed data" };

			std::memcpy(outCompressed.data() + oldSize, pZctx->internalBuf.data(), endSize);
		}

		return { true, "" };
	}

	std::unique_ptr<IDecompressionContext> CLZ4CompressionStrategy::BeginDecompression() const noexcept
	{
		auto pCtx = std::make_unique<SLZ4DecompressionContext>();
		return (!pCtx->dctx || pCtx->internalBuf.empty()) ? nullptr : std::move(pCtx);
	}

	Data::SRunResult CLZ4CompressionStrategy::DecompressChunk(IDecompressionContext& ctx, const void* pSrc, size_t srcSize, std::vector<uint8>& outDecompressed) const noexcept
	{
		auto* pZctx = static_cast<SLZ4DecompressionContext*>(&ctx);
		if (!pZctx)
			return { false, "Internal Error: Failed to cast LZ4 decompression context" };

		if (srcSize == 0)
			return { true, "" };

		size_t processedBytes = 0;
		const uint8* pRawData = static_cast<const uint8*>(pSrc);

		while (processedBytes < srcSize)
		{
			size_t srcSizeLeft = srcSize - processedBytes;
			size_t dstSize = pZctx->internalBuf.size();

			const size_t ret = LZ4F_decompress(pZctx->dctx, pZctx->internalBuf.data(), &dstSize, pRawData + processedBytes, &srcSizeLeft, nullptr);
			if (LZ4F_isError(ret))
				return { false, std::string("LZ4 decompression failed: ") + LZ4F_getErrorName(ret) };

			if (dstSize > 0)
			{
				const size_t oldSize = outDecompressed.size();
				if (!Utils::Common::TryResize(outDecompressed, oldSize + dstSize))
					return { false, "Out of Memory: Failed to append uncompressed data" };

				std::memcpy(outDecompressed.data() + oldSize, pZctx->internalBuf.data(), dstSize);
			}
			processedBytes += srcSizeLeft;
		}

		return { true, "" };
	}
}