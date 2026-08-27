#include "StdAfx.h"
#include "ImageImporter.h"

#include "PNGStreamReader.h"
#include "Core/Data/RunResult.h"
#include "Core/Bakers/IMapBaker.h"
#include "Core/FileSystem/PathResolver.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/ScopedCryFile.h"
#include "Utils/ImageView.h"
#include "Utils/Progress.h"

namespace JDKLevelMaps::ImageWork
{
	Data::SRunResult CImageImporter::Prepare(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor)
	{
		if (auto imagePath = pathResolver.GetImagePath(baker.GetID()))
			m_imagePath = imagePath.value();
		else
			return { false, "Disk I/O Error: Cannot get image path" };

		Utils::FileSystem::ScopedCryFile file(FileSystem::LFSFacade::FOpen(m_imagePath.c_str(), "rb"));
		if (!file)
			return { false, "Can't open preview image file for reading" };

		Data::SRunResult result = ReadPNGInfo(file, SImageSizes{ m_imageWidth, m_imageHeight });
		if (!result.bSuccess)
			return result;

		if (pProgressor)
			m_pImageTask = pProgressor->RegisterProgressTask(m_imageHeight, 1);

		m_bReady = true;
		return { false, "" };
	}

	Data::SRunResult CImageImporter::ImportImage(SImageView& outImageView)
	{
		if (!m_bReady)
			return { false, "Importer is not ready" };
		m_bReady = false;

		Utils::FileSystem::ScopedCryFile file(FileSystem::LFSFacade::FOpen(m_imagePath.c_str(), "rb"));
		if (!file)
			return { false, "Can't open preview image file for reading" };

		if (!outImageView.Resize(m_imageWidth, m_imageHeight))
			return { false, "Failed to resize destination image view" };

		return ReadPNG(file, SImageSizes{ m_imageWidth, m_imageHeight }, outImageView, m_pImageTask);
	}
}