#pragma once
#include <string>

#include "PNGStreamReader.h"

namespace JDKLevelMaps::Data
{
	struct SLevelContext;
	struct SRunResult;
}

namespace JDKLevelMaps::FileSystem
{
	class CPathResolver;
}

namespace JDKLevelMaps::Bakers
{
	class IMapBaker;
}

namespace JDKLevelMaps::Utils::Common
{
	class CProgressor;
	struct SProgressTask;
}

namespace JDKLevelMaps::ImageWork
{
	struct SImageView;

	class CImageImporter final
	{
	public:
		[[nodiscard]] Data::SRunResult Prepare(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor);

		[[nodiscard]] Data::SRunResult ImportImage(SImageView& outImageView);

	private:
		Utils::Common::SProgressTask* m_pImageTask = nullptr;

		SImageSizes m_imageSizes;
		std::string m_imagePath;

		bool m_bReady = false;
	};
}