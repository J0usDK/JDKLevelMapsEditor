#include "StdAfx.h"
#include "ImageLoader.h"

#include "Core/BakersRegistry.h"
#include "Core/Data/RunResult.h"
#include "Core/Bakers/IMapBaker.h"
#include "Core/FileSystem/PathResolver.h"
#include "Core/MapWork/MapFileReader.h"
#include "Core/ImageWork/Import/ImageImporter.h"
#include "Utils/Progress.h"

namespace JDKLevelMaps::Managers
{
	CImageLoader::CImageLoader(CBakersRegistry& bakersRegistry, FileSystem::CPathResolver& pathResolver) noexcept
		: m_pathResolver(pathResolver), m_bakersRegistry(bakersRegistry) { }

	Data::SRunResult CImageLoader::LoadPreviewFromMap(EMapType bakerType, Utils::Common::SProgress& progress, ImageWork::SImageView& outImage)
	{
		const auto pBaker = m_bakersRegistry.GetBaker(bakerType);
		if (!pBaker)
			return { false, "Cannot get baker" };

		MapWork::CMapFileReader mapFileReader;
		Utils::Common::CProgressor progressor(progress);
		Data::SRunResult result;

		result = mapFileReader.Prepare(*pBaker, m_pathResolver, &progressor);
		if (!result.bSuccess)
			return result;

		return mapFileReader.LoadPreviewFromMap(outImage);
	}

	Data::SRunResult CImageLoader::LoadPreviewFromDisk(EMapType bakerType, Utils::Common::SProgress& progress, ImageWork::SImageView& outImage)
	{
		const auto pBaker = m_bakersRegistry.GetBaker(bakerType);
		if (!pBaker)
			return { false, "Cannot get baker" };

		ImageWork::CImageImporter imageReader;
		Utils::Common::CProgressor progressor(progress);
		Data::SRunResult result;

		result = imageReader.Prepare(*pBaker, m_pathResolver, &progressor);
		if (!result.bSuccess)
			return result;

		return imageReader.ImportImage(outImage);
	}
}