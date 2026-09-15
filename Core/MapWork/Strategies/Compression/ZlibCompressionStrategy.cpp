#include "StdAfx.h"
#include "ZlibCompressionStrategy.h"

#include <QtZlib/zlib.h>

#include "Core/Data/RunResult.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	struct SZlibCompressionContext final : public ICompressionContext
	{
		z_stream stream = {};
		std::vector<uint8> internalBuf;

		SZlibCompressionContext()
		{
			Utils::Common::TryResize(internalBuf, 128 * 1024); // 128 KB
		}

		~SZlibCompressionContext() override
		{
			deflateEnd(&stream);
		}
	};

	struct SZlibDecompressionContext final : public IDecompressionContext
	{
		z_stream stream = {};
		std::vector<uint8> internalBuf;

		SZlibDecompressionContext()
		{
			inflateInit(&stream);
			Utils::Common::TryResize(internalBuf, 128 * 1024);
		}
		~SZlibDecompressionContext() override
		{
			inflateEnd(&stream);
		}
	};

	size_t CZlibCompressionStrategy::GetMaxCompressedSize(size_t rawSize) const noexcept
	{
		return static_cast<size_t>(compressBound(static_cast<uLong>(rawSize)));
	}

	Data::SRunResult CZlibCompressionStrategy::Compress(const void* pSrc, size_t srcSize, void* pDst, size_t dstCapacity, size_t& outCompressedSize) const noexcept
	{
		uLongf destLen = static_cast<uLongf>(dstCapacity);
		const int result = compress(static_cast<Bytef*>(pDst), &destLen, static_cast<const Bytef*>(pSrc), static_cast<uLong>(srcSize));

		if (result != Z_OK)
			return { false, "Zlib compression failed with code:" + std::to_string(result) };

		outCompressedSize = static_cast<size_t>(destLen);
		return { true, "" };
	}

	Data::SRunResult CZlibCompressionStrategy::Decompress(const void* pSrc, size_t compressedSize, void* pDst, size_t rawSize) const noexcept
	{
		uLongf destLen = static_cast<uLongf>(rawSize);
		const int result = uncompress(static_cast<Bytef*>(pDst), &destLen, static_cast<const Bytef*>(pSrc), static_cast<uLong>(compressedSize));

		if (result != Z_OK)
			return { false, "Zlib decompression failed with code: " + std::to_string(result) };

		if (static_cast<size_t>(destLen) != rawSize)
			return { false, "Zlib decompression failed: Size mismatch" };

		return { true, "" };
	}

	std::unique_ptr<ICompressionContext> CZlibCompressionStrategy::BeginCompression() const noexcept
	{
		auto pCtx = std::make_unique<SZlibCompressionContext>();
		if (pCtx->internalBuf.empty())
			return nullptr;

		if (deflateInit(&pCtx->stream, Z_DEFAULT_COMPRESSION) != Z_OK)
			return nullptr;

		return pCtx;
	}

	Data::SRunResult CZlibCompressionStrategy::CompressChunk(ICompressionContext& ctx, const void* pSrc, size_t srcSize, std::vector<uint8>& outCompressed) const noexcept
	{
		auto* pZctx = static_cast<SZlibCompressionContext*>(&ctx);
		if (!pZctx)
			return { false, "Internal Error: Failed to cast Zlib compression context" };

		if (srcSize == 0)
			return { true, "" };

		pZctx->stream.avail_in = static_cast<uInt>(srcSize);
		pZctx->stream.next_in = static_cast<Bytef*>(const_cast<void*>(pSrc));

		do
		{
			pZctx->stream.avail_out = static_cast<uInt>(pZctx->internalBuf.size());
			pZctx->stream.next_out = pZctx->internalBuf.data();

			if (deflate(&pZctx->stream, Z_NO_FLUSH) == Z_STREAM_ERROR)
				return { false, "Zlib stream compression failed" };

			const size_t have = pZctx->internalBuf.size() - pZctx->stream.avail_out;

			if (have > 0)
			{
				const size_t oldSize = outCompressed.size();
				if (!Utils::Common::TryResize(outCompressed, oldSize + have))
					return { false, "Out of Memory: Failed to append compressed data" };

				std::memcpy(outCompressed.data() + oldSize, pZctx->internalBuf.data(), have);
			}
		} while (pZctx->stream.avail_out == 0);

		return { true, "" };
	}

	Data::SRunResult CZlibCompressionStrategy::EndCompression(ICompressionContext& ctx, std::vector<uint8>& outCompressed) const noexcept
	{
		auto* pZctx = static_cast<SZlibCompressionContext*>(&ctx);
		if (!pZctx)
			return { false, "Internal Error: Failed to cast Zlib compression context" };

		pZctx->stream.avail_in = 0;
		pZctx->stream.next_in = nullptr;

		int ret;
		do
		{
			pZctx->stream.avail_out = static_cast<uInt>(pZctx->internalBuf.size());
			pZctx->stream.next_out = pZctx->internalBuf.data();

			ret = deflate(&pZctx->stream, Z_FINISH);
			if (ret == Z_STREAM_ERROR)
				return { false, "Zlib stream end failed" };

			const size_t have = pZctx->internalBuf.size() - pZctx->stream.avail_out;

			if (have > 0)
			{
				const size_t oldSize = outCompressed.size();
				if (!Utils::Common::TryResize(outCompressed, oldSize + have))
					return { false, "Out of Memory: Failed to append final compressed data" };

				std::memcpy(outCompressed.data() + oldSize, pZctx->internalBuf.data(), have);
			}
		} while (ret != Z_STREAM_END);

		return { true, "" };
	}

	std::unique_ptr<IDecompressionContext> CZlibCompressionStrategy::BeginDecompression() const noexcept
	{
		auto pCtx = std::make_unique<SZlibDecompressionContext>();
		return pCtx->internalBuf.empty() ? nullptr : std::move(pCtx);
	}

	Data::SRunResult CZlibCompressionStrategy::DecompressChunk(IDecompressionContext& ctx, const void* pSrc, size_t srcSize, std::vector<uint8>& outDecompressed) const noexcept
	{
		auto* pZctx = static_cast<SZlibDecompressionContext*>(&ctx);
		if (!pZctx)
			return { false, "Internal Error: Failed to cast Zlib decompression context" };

		if (srcSize == 0)
			return { true, "" };

		pZctx->stream.avail_in = static_cast<uInt>(srcSize);
		pZctx->stream.next_in = static_cast<Bytef*>(const_cast<void*>(pSrc));

		do
		{
			pZctx->stream.avail_out = static_cast<uInt>(pZctx->internalBuf.size());
			pZctx->stream.next_out = pZctx->internalBuf.data();

			int ret = inflate(&pZctx->stream, Z_NO_FLUSH);
			if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
				return { false, "Zlib decompression corrupted data" };

			const size_t have = pZctx->internalBuf.size() - pZctx->stream.avail_out;
			if (have > 0)
			{
				const size_t oldSize = outDecompressed.size();
				if (!Utils::Common::TryResize(outDecompressed, oldSize + have))
					return { false, "Out of Memory: Failed to append uncompressed data" };

				std::memcpy(outDecompressed.data() + oldSize, pZctx->internalBuf.data(), have);
			}
		} while (pZctx->stream.avail_out == 0);

		return { true, "" };
	}
}