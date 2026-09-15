#pragma once
#include <variant>
#include <vector>

#include "Core/Data/MapContext.h"
#include "Core/MapWork/Strategies/DirectoryFormat/DirectoryBitmaskStrategy.h"
#include "Core/MapWork/Strategies/DirectoryFormat/DirectoryHybridStrategy.h"
#include "Core/MapWork/Strategies/Compression/ZlibCompressionStrategy.h"
#include "Core/MapWork/Strategies/Compression/ZstdCompressionStrategy.h"
#include "Core/MapWork/Strategies/Compression/LZ4CompressionStrategy.h"

namespace JDKLevelMaps::Data
{
	struct SRunResult;
}

namespace JDKLevelMaps::FileSystem
{
	class CPathResolver;
}

namespace JDKLevelMaps::Utils::Common
{
	class CProgressor;
	struct SProgressTask;
}

namespace JDKLevelMaps::ImageWork
{
	struct SImageView;
}

namespace JDKLevelMaps::ImageWork::Converters
{
	struct SConvertContext;
}

namespace JDKLevelMaps::MapWork
{
	struct SDecompressor;

	class CMapFileReader final
	{
	public:
		[[nodiscard]] Data::SRunResult Prepare(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor);
		[[nodiscard]] Data::SRunResult LoadPreviewFromMap(ImageWork::SImageView& outImage);

	private:
		// PREPARE PHASE

		[[nodiscard]] Data::SRunResult BuildContext(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver);
		[[nodiscard]] Data::SRunResult ReadMapHeader(FILE* pFile);
		[[nodiscard]] Data::SRunResult InitFormatStrategy() noexcept;
		[[nodiscard]] Data::SRunResult InitCompressStrategy() noexcept;
		void InitProgressTasks(Utils::Common::CProgressor* pProgressor);

	private:
		// HELPERS

		[[nodiscard]] const Strategies::ICompressionStrategy* GetDecompressor() const noexcept;
		[[nodiscard]] Strategies::IDirectoryFormatStrategy* GetFormat() noexcept;

	private:
		// READ PHASE

		[[nodiscard]] Data::SRunResult ReadMap(FILE* pFile, ImageWork::SImageView& outImage);
		[[nodiscard]] Data::SRunResult ReadDirectory(FILE* pFile, std::vector<uint8>& outDirectory);
		[[nodiscard]] Data::SRunResult ReadTiles(FILE* pFile, const std::vector<uint8>& directory, ImageWork::SImageView& outImage);
		[[nodiscard]] Data::SRunResult ReadTile(uint64 tileIndex, FILE* pFile, std::vector<uint8>& tileBuffer, SDecompressor& decompressor, ImageWork::Converters::SConvertContext& convertCtx, uint64 realTileSize);

	private:
		using TBitmaskStrategy = Strategies::CDirectoryBitmaskStrategy;
		using THybrid32Strategy = Strategies::CDirectoryHybridStrategy<uint32>;
		using THybrid64Strategy = Strategies::CDirectoryHybridStrategy<uint64>;

		using TZlibCompressStrategy = Strategies::CZlibCompressionStrategy;
		using TZstdCompressStrategy = Strategies::CZstdCompressionStrategy;
		using TLZ4CompressStrategy = Strategies::CLZ4CompressionStrategy;

	private:
		Data::SMapReadContext m_readContext;

		std::variant<std::monostate, TBitmaskStrategy, THybrid32Strategy, THybrid64Strategy> m_formatStrategy;
		std::variant<std::monostate, TZlibCompressStrategy, TZstdCompressStrategy, TLZ4CompressStrategy> m_decompressStrategy;

		bool m_bReady = false;
	};
}