#include "StdAfx.h"
#include "MapFileWriter.h"

#include <utility>

#include "Core/Data/LevelContext.h"
#include "Core/Data/RunResult.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/ScopedCryFile.h"

namespace JDKLevelMaps::MapWork
{
	Data::SRunResult CMapFileWriter::Prepare(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor, BakeData bakingData, bool bUseHybrid)
	{
		if (auto result = BuildBakeContext(baker, pathResolver, context, bakingData, bUseHybrid); !result.bSuccess)
			return result;

		InitDirectoryStrategy();

		InitProgressTasks(context, pProgressor);

		m_bReady = true;
		return { true, "" };
	}

	Data::SRunResult CMapFileWriter::BakeMap(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, BakeData bakingData)
	{
		if (!std::exchange(m_bReady, false))
			return { false, "Internal Error: Map Writer is not ready" };

		Utils::FileSystem::ScopedCryFile file(FileSystem::LFSFacade::FOpen(m_bakeContext.mapPath.c_str(), "wb", true), m_bakeContext.mapPath.c_str());
		if (!file)
			return { false, "Disk I/O Error: Cannot open map file for writing" };

		if (!WriteHeader(context, file))
			return { false, "Disk I/O Error: Cannot write map's header" };

		Data::SRunResult result;
		switch (m_bakeContext.entryFormat)
		{
		case ETileEntryFormat::Bitmask:
			result = WriteMap(context, file, bakingData);
			break;
		case ETileEntryFormat::Hybrid_32:
			result = WriteMap<uint32>(context, file, bakingData);
			break;
		case ETileEntryFormat::Hybrid_64:
			result = WriteMap<uint64>(context, file, bakingData);
			break;
		}

		if (!result.bSuccess)
			return result;

		file.close(true);
		return { true, "Map was written to: " + m_bakeContext.mapPath };
	}
}