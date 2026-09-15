#pragma once
#include "ICompressionStrategy.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	class CLZ4CompressionStrategy final : public ICompressionStrategy
	{
	public:
		[[nodiscard]] size_t GetMaxCompressedSize(size_t rawSize) const noexcept override;
		[[nodiscard]] Data::SRunResult Compress(const void* pSrc, size_t srcSize, void* pDst, size_t dstCapacity, size_t& outCompressedSize) const noexcept override;
		[[nodiscard]] Data::SRunResult Decompress(const void* pSrc, size_t compressedSize, void* pDst, size_t rawSize) const noexcept override;
	
		[[nodiscard]] std::unique_ptr<ICompressionContext> BeginCompression() const noexcept override;
		[[nodiscard]] Data::SRunResult CompressChunk(ICompressionContext& ctx, const void* pSrc, size_t srcSize, std::vector<uint8>& outCompressed) const noexcept override;
		[[nodiscard]] Data::SRunResult EndCompression(ICompressionContext& ctx, std::vector<uint8>& outCompressed) const noexcept override;
	
		[[nodiscard]] std::unique_ptr<IDecompressionContext> BeginDecompression() const noexcept override;
		[[nodiscard]] Data::SRunResult DecompressChunk(IDecompressionContext& ctx, const void* pSrc, size_t srcSize, std::vector<uint8>& outDecompressed) const noexcept override;
	};
}