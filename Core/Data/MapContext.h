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

		EMapType mapType = EMapType::VegetationDensity;
		ETileEntryFormat entryFormat = ETileEntryFormat::Bitmask;

		std::string mapPath = "";

		uint32 channelsCount = 0;
		uint32 layersMask = 0;
		uint32 tileCountX = 0;
		uint32 tileCountY = 0;
		uint64 totalTiles = 0;

		uint64 directoryOffset = 0;
		uint64 tilesOffset = 0;
		uint64 nonEmptyTilesCount = 0;

		std::vector<uint64> bitmask;
	};

	struct SMapReadContext
	{
		Utils::Common::SProgressTask* pReadTask = nullptr;
		Bakers::DebugColorMapperPtr pColorMapper = nullptr;

		SMapHeader header;
		uint32 channelsCount = 0;
		uint64 directoryOffset = 0;

		std::string mapPath;
	};
}