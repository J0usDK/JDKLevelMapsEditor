#pragma once
#include <string>
#include <vector>

#include "Core/Bakers/IMapBaker.h"
#include "Shared/MapHeader.h"

namespace JDKLevelMaps::Utils::Common
{
	struct SProgressTask;
}

namespace JDKLevelMaps::Data
{
	struct SMapWriteContext
	{
		Utils::Common::SProgressTask* pTilesTask = nullptr;
		Utils::Common::SProgressTask* pDirectoryTask = nullptr;
		Utils::Common::SProgressTask* pDirectoryFlushTask = nullptr;

		EMapType mapType = EMapType::VegetationDensity;
		std::string mapPath = "";

		uint64 tilesOffset = 0;

		uint32 channelsCount = 0;
		uint32 layersMask = 0;
		uint32 tileCountX = 0;
		uint32 tileCountY = 0;
		uint64 totalTiles = 0;

		uint64 nonEmptyTilesCount = 0;

		std::vector<uint64> bitmask;
	};

	struct SMapReadContext
	{
		Utils::Common::SProgressTask* pDirTask = nullptr;
		Utils::Common::SProgressTask* pTilesTask = nullptr;
		Bakers::DebugColorMapperPtr pColorMapper = nullptr;

		SMapHeader header;
		uint32 channelsCount = 0;

		std::string mapPath;
	};
}