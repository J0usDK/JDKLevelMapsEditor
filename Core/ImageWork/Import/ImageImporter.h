#pragma once
#include <string>

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

		
		uint32 m_imageWidth = 0;
		uint32 m_imageHeight = 0;
		std::string m_imagePath;

		bool m_bReady = false;
	};
}