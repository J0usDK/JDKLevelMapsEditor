#include "StdAfx.h"
#include "ZstdCompressionStrategy.h"

#include <Includes/Zstd/zstd.h>

#include "Core/Data/RunResult.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	struct SZstdCompressionContext final : public ICompressionContext
	{
		ZSTD_CCtx* cctx = nullptr;
		std::vector<uint8> internalBuf;

		SZstdCompressionContext()
		{
			cctx = ZSTD_createCCtx();
			Utils::Common::TryResize(internalBuf, ZSTD_CStreamOutSize());
		}

		~SZstdCompressionContext() override
		{
			if (cctx)
				ZSTD_freeCCtx(cctx);
		}
	};

	struct SZstdDecompressionContext final : public IDecompressionContext
	{
		ZSTD_DCtx* dctx = nullptr;
		std::vector<uint8> internalBuf;

		SZstdDecompressionContext()
		{
			dctx = ZSTD_createDCtx();
			Utils::Common::TryResize(internalBuf, ZSTD_DStreamOutSize());
		}

		~SZstdDecompressionContext() override
		{
			if (dctx)
				ZSTD_freeDCtx(dctx);
		}
	};

	size_t CZstdCompressionStrategy::GetMaxCompressedSize(size_t rawSize) const noexcept
	{
		return ZSTD_compressBound(rawSize);
	}

	Data::SRunResult CZstdCompressionStrategy::Compress(const void* pSrc, size_t srcSize, void* pDst, size_t dstCapacity, size_t& outCompressedSize) const noexcept
	{
		const size_t result = ZSTD_compress(pDst, dstCapacity, pSrc, srcSize, 3);

		if (ZSTD_isError(result))
			return { false, std::string("Zstd compression failed: ") + ZSTD_getErrorName(result) };

		outCompressedSize = result;
		return { true, "" };
	}

	Data::SRunResult CZstdCompressionStrategy::Decompress(const void* pSrc, size_t compressedSize, void* pDst, size_t rawSize) const noexcept
	{
		const size_t result = ZSTD_decompress(pDst, rawSize, pSrc, compressedSize);

		if (ZSTD_isError(result))
			return { false, std::string("Zstd decompression failed: ") + ZSTD_getErrorName(result) };

		if (result != rawSize)
			return { false, "Zstd decompression failed: Size mismatch" };

		return { true, "" };
	}

	std::unique_ptr<ICompressionContext> CZstdCompressionStrategy::BeginCompression() const noexcept
	{
		auto pCtx = std::make_unique<SZstdCompressionContext>();
		if (!pCtx->cctx || pCtx->internalBuf.empty())
			return nullptr;

		return pCtx;
	}

	Data::SRunResult CZstdCompressionStrategy::CompressChunk(ICompressionContext& ctx, const void* pSrc, size_t srcSize, std::vector<uint8>& outCompressed) const noexcept
	{
		auto* pZctx = static_cast<SZstdCompressionContext*>(&ctx);
		if (!pZctx)
			return { false, "Internal Error: Failed to cast Zstd compression context" };

		if (srcSize == 0)
			return { true, "" };

		ZSTD_inBuffer input = { pSrc, srcSize, 0 };

		while (input.pos < input.size)
		{
			ZSTD_outBuffer output = { pZctx->internalBuf.data(), pZctx->internalBuf.size(), 0 };
			const size_t ret = ZSTD_compressStream2(pZctx->cctx, &output, &input, ZSTD_e_continue);

			if (ZSTD_isError(ret))
				return { false, std::string("Zstd stream error: ") + ZSTD_getErrorName(ret) };

			if (output.pos > 0)
			{
				const size_t oldSize = outCompressed.size();
				if (!Utils::Common::TryResize(outCompressed, oldSize + output.pos))
					return { false, "Out of Memory: Failed to append compressed data" };

				std::memcpy(outCompressed.data() + oldSize, pZctx->internalBuf.data(), output.pos);
			}
		}

		return { true, "" };
	}

	Data::SRunResult CZstdCompressionStrategy::EndCompression(ICompressionContext& ctx, std::vector<uint8>& outCompressed) const noexcept
	{
		auto* pZctx = static_cast<SZstdCompressionContext*>(&ctx);
		if (!pZctx)
			return { false, "Internal Error: Failed to cast Zstd compression context" };

		ZSTD_inBuffer input = { nullptr, 0, 0 };

		size_t ret;
		do
		{
			ZSTD_outBuffer output = { pZctx->internalBuf.data(), pZctx->internalBuf.size(), 0 };
			ret = ZSTD_compressStream2(pZctx->cctx, &output, &input, ZSTD_e_end);

			if (ZSTD_isError(ret))
				return { false, std::string("Zstd compression stream error: ") + ZSTD_getErrorName(ret) };

			if (output.pos > 0)
			{
				const size_t oldSize = outCompressed.size();
				if (!Utils::Common::TryResize(outCompressed, oldSize + output.pos))
					return { false, "Out of Memory: Failed to append final compressed data" };

				std::memcpy(outCompressed.data() + oldSize, pZctx->internalBuf.data(), output.pos);
			}
		} while (ret > 0);

		return { true, "" };
	}

	std::unique_ptr<IDecompressionContext> CZstdCompressionStrategy::BeginDecompression() const noexcept
	{
		auto pCtx = std::make_unique<SZstdDecompressionContext>();
		return (!pCtx->dctx || pCtx->internalBuf.empty()) ? nullptr : std::move(pCtx);
	}

	Data::SRunResult CZstdCompressionStrategy::DecompressChunk(IDecompressionContext& ctx, const void* pSrc, size_t srcSize, std::vector<uint8>& outDecompressed) const noexcept
	{
		auto* pZctx = static_cast<SZstdDecompressionContext*>(&ctx);
		if (!pZctx)
			return { false, "Internal Error: Failed to cast Zstd decompression context" };

		if (srcSize == 0)
			return { true, "" };

		ZSTD_inBuffer input = { pSrc, srcSize, 0 };
		while (input.pos < input.size)
		{
			ZSTD_outBuffer output = { pZctx->internalBuf.data(), pZctx->internalBuf.size(), 0 };
			const size_t ret = ZSTD_decompressStream(pZctx->dctx, &output, &input);

			if (ZSTD_isError(ret))
				return { false, std::string("Zstd decompression stream error: ") + ZSTD_getErrorName(ret) };

			if (output.pos > 0)
			{
				const size_t oldSize = outDecompressed.size();
				if (!Utils::Common::TryResize(outDecompressed, oldSize + output.pos))
					return { false, "Out of Memory: Failed to append uncompressed data" };

				std::memcpy(outDecompressed.data() + oldSize, pZctx->internalBuf.data(), output.pos);
			}
		}

		return { true, "" };
	}
}