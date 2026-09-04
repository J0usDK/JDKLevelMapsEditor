#include "StdAfx.h"
#include "MapsBaker.h"

#include <future>

#include "Core/Data/LevelContext.h"
#include "Core/Data/RunResult.h"
#include "Core/BakersRegistry.h"
#include "Core/Bakers/IMapBaker.h"
#include "Core/FileSystem/PathResolver.h"
#include "Core/MapWork/Write/MapFileWriter.h"
#include "Core/ImageWork/Export/ImageExporter.h"
#include "Settings/BakerSettings.h"
#include "Utils/Progress.h"

namespace JDKLevelMaps::Managers
{
	CMapsBaker::CMapsBaker(CBakersRegistry& bakersRegistry, FileSystem::CPathResolver& pathResolver, const Settings::SBakerSettings& bakerSettings) noexcept
		: m_bakersRegistry(bakersRegistry), m_pathResolver(pathResolver), m_bakerSettings(bakerSettings) { }

	Data::SRunResult CMapsBaker::RunBake(EMapType bakerType, Utils::Common::SProgress& progress)
	{
		const auto pBaker = m_bakersRegistry.GetBaker(bakerType);
		if (!pBaker)
			return { false, "Cannot get baker" };

		Data::SLevelContext context = Data::ComputeLevelContext(m_bakerSettings.cellSize, m_bakerSettings.tileSize);
		std::vector<uint8> bakingData = pBaker->Bake(context);

		MapWork::CMapFileWriter mapFileWriter;
		ImageWork::CImageExporter imageExporter;
		Utils::Common::CProgressor progressor(progress);

		bool bUseHybrid = m_bakerSettings.directoryFormat == Settings::EDirectoryFormat::Hybrid;

		if (auto result = mapFileWriter.Prepare(*pBaker, context, m_pathResolver, &progressor, bakingData, bUseHybrid); !result.bSuccess)
			return result;

		if (m_bakerSettings.bGenerateDebugImage)
			if (auto result = imageExporter.Prepare(*pBaker, context, m_pathResolver, &progressor); !result.bSuccess)
				return result;

		std::future<Data::SRunResult> mapWriteTask = std::async(std::launch::async, [&]()
			{ return mapFileWriter.BakeMap(*pBaker, context, bakingData); });

		std::future<Data::SRunResult> imageExportTask = std::async(std::launch::async, [&]()
			{ return m_bakerSettings.bGenerateDebugImage ? imageExporter.ExportImage(*pBaker, context, bakingData) : Data::SRunResult(true, ""); });

		Data::SRunResult mapResult = mapWriteTask.get();
		Data::SRunResult imageResult = imageExportTask.get();

		if (mapResult.bSuccess && imageResult.bSuccess)
			return { true, "" };
		if (!mapResult.bSuccess)
			return mapResult;

		return imageResult;
	}
}