#pragma once
#include <variant>

#include "Core/Data/MapContext.h"
#include "Core/MapWork/Strategies/DirectoryBitmaskStrategy.h"
#include "Core/MapWork/Strategies/DirectoryHybridStrategy.h"

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
	class CMapFileReader final
	{
	public:
		[[nodiscard]] Data::SRunResult Prepare(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor);
		[[nodiscard]] Data::SRunResult LoadPreviewFromMap(ImageWork::SImageView& outImage);

	private:
		[[nodiscard]] Data::SRunResult BuildContext(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver);
		[[nodiscard]] Data::SRunResult ReadMapHeader(FILE* pFile);
		[[nodiscard]] Data::SRunResult InitFormatStrategy() noexcept;

	private:
		[[nodiscard]] Data::SRunResult ReadTiles(FILE* pFile, ImageWork::SImageView& outImage);
		[[nodiscard]] Data::SRunResult ReadTile(uint64 tileIndex, FILE* pFile, std::vector<uint8>& tileBuffer, ImageWork::Converters::SConvertContext& convertCtx);

	private:
		using TBitmaskStrategy = Strategies::CDirectoryBitmaskStrategy;
		using THybrid32Strategy = Strategies::CDirectoryHybridStrategy<uint32>;
		using THybrid64Strategy = Strategies::CDirectoryHybridStrategy<uint64>;

	private:
		Data::SMapReadContext m_readContext;
		std::variant<std::monostate, TBitmaskStrategy, THybrid32Strategy, THybrid64Strategy> m_strategy;

		bool m_bReady = false;
	};
}