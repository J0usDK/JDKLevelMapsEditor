#include "StdAfx.h"
#include "MapFileReader.h"

#include <utility>

#include "Core/Data/RunResult.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/Progress.h"
#include "Utils/ScopedCryFile.h"
#include "Utils/ImageView.h"

namespace JDKLevelMaps::MapWork
{
	Data::SRunResult CMapFileReader::Prepare(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor)
	{
		if (auto result = BuildContext(baker, pathResolver); !result.bSuccess)
			return result;

		Utils::FileSystem::ScopedCryFile file(FileSystem::LFSFacade::FOpen(m_readContext.mapPath.c_str(), "rb"));
		if (!file)
			return { false, "Disk I/O Error: Cannot open map file for reading" };

		if (auto result = ReadMapHeader(file); !result.bSuccess)
			return result;

		if (auto result = InitFormatStrategy(); !result.bSuccess)
			return result;

		if (pProgressor)
		{
			const uint64 totalTiles = static_cast<uint64>(m_readContext.header.tileCountX) * m_readContext.header.tileCountY;
			m_readContext.pReadTask = pProgressor->RegisterProgressTask(totalTiles, 1);
		}

		m_bReady = true;
		return { true, "" };
	}

	Data::SRunResult CMapFileReader::LoadPreviewFromMap(ImageWork::SImageView& outImage)
	{
		if (!std::exchange(m_bReady, false))
			return { false, "Internal Error: Map Reader is not ready" };

		Utils::FileSystem::ScopedCryFile file(FileSystem::LFSFacade::FOpen(m_readContext.mapPath.c_str(), "rb"));
		if (!file)
			return { false, "Disk I/O Error: Cannot open map file for reading" };

		outImage.Resize(m_readContext.header.gridWidth, m_readContext.header.gridHeight);
		if (!outImage.IsValid())
			return { false, "Out of Memory: Failed to allocate QImage" };

		return ReadTiles(file, outImage);
	}
}