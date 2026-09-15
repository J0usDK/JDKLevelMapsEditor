#pragma once

namespace JDKLevelMaps::Data
{
	struct SRunResult;
}

namespace JDKLevelMaps::MapWork::Strategies
{
	struct ICompressionContext
	{
		virtual ~ICompressionContext() = default;
	};

	struct IDecompressionContext
	{
		virtual ~IDecompressionContext() = default;
	};

	class ICompressionStrategy
	{
	public:
		virtual ~ICompressionStrategy() = default;

		[[nodiscard]] virtual size_t GetMaxCompressedSize(size_t rawSize) const noexcept = 0;
		[[nodiscard]] virtual Data::SRunResult Compress(const void* pSrc, size_t srcSize, void* pDst, size_t dstCapacity, size_t& outCompressedSize) const noexcept = 0;
		[[nodiscard]] virtual Data::SRunResult Decompress(const void* pSrc, size_t compressedSize, void* pDst, size_t rawSize) const noexcept = 0;

		[[nodiscard]] virtual std::unique_ptr<ICompressionContext> BeginCompression() const noexcept = 0;
		[[nodiscard]] virtual Data::SRunResult CompressChunk(ICompressionContext& ctx, const void* pSrc, size_t srcSize, std::vector<uint8>& outCompressed) const noexcept = 0;
		[[nodiscard]] virtual Data::SRunResult EndCompression(ICompressionContext& ctx, std::vector<uint8>& outCompressed) const noexcept = 0;

		[[nodiscard]] virtual std::unique_ptr<IDecompressionContext> BeginDecompression() const noexcept = 0;
		[[nodiscard]] virtual Data::SRunResult DecompressChunk(IDecompressionContext& ctx, const void* pSrc, size_t srcSize, std::vector<uint8>& outDecompressed) const noexcept = 0;
	};
}