#include "StdAfx.h"
#include "MapFileReader.h"

#include <CrySystem/File/ICryPak.h>

#include "Core/Data/RunResult.h"
#include "Core/Bakers/IMapBaker.h"
#include "Core/FileSystem/PathResolver.h"

namespace JDKLevelMaps::MapWork
{
	Data::SRunResult CMapFileReader::BuildContext(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver)
	{
		if (auto mapPath = pathResolver.GetMapPath(baker.GetID()))
			m_readContext.mapPath = std::move(*mapPath);
		else
			return { false, "Disk I/O Error: Cannot get map's path" };

		m_readContext.pColorMapper = baker.GetDebugColorMapper();
		return { true, "" };
	}

	Data::SRunResult CMapFileReader::ReadMapHeader(FILE* pFile)
	{
		if (gEnv->pCryPak->FReadRaw(&m_readContext.header, sizeof(SMapHeader), 1, pFile) != 1)
			return { false, "Disk I/O Error: Cannot read map's header" };

		m_readContext.directoryOffset = sizeof(SMapHeader);

		if (!IsValidMapHeader(m_readContext.header))
			return { false, "Invalid map file (file is corrupted?)" };

		m_readContext.channelsCount = static_cast<uint32>(std::bitset<8>(m_readContext.header.activeLayersMask).count());
		if (m_readContext.channelsCount == 0)
			return { false, "Invalid map file: No active layers found in header" };

		return { true, "" };
	}

	Data::SRunResult CMapFileReader::InitFormatStrategy() noexcept
	{
		switch (m_readContext.header.entryFormat)
		{
		case ETileEntryFormat::Bitmask:
			m_strategy.emplace<TBitmaskStrategy>();
			return { true, "" };
		case ETileEntryFormat::Hybrid_32:
			m_strategy.emplace<THybrid32Strategy>();
			return { true, "" };
		case ETileEntryFormat::Hybrid_64:
			m_strategy.emplace<THybrid64Strategy>();
			return { true, "" };
		default:
			return { false, "Unknown or unsupported map directory format" };
		}
	}
}