#pragma once
#include <vector>
#include <string>
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps::Utils::Common
{
	class CProgressor;
	struct SProgressTask;
}

namespace JDKLevelMaps::Bakers
{
	class IMapBaker;
}


namespace JDKLevelMaps::Data
{
	struct SRunResult;
	struct SLevelContext;
}

namespace JDKLevelMaps::FileSystem
{
	class CPathResolver;
}

namespace JDKLevelMaps::ImageWork
{
	class CImageExporter
	{
	public:
		[[nodiscard]] Data::SRunResult Prepare(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor);
		
		[[nodiscard]] Data::SRunResult ExportImage(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, const std::vector<uint8>& data);

	private:
		Utils::Common::SProgressTask* m_pImageTask = nullptr;

		bool m_bReady = false;
		std::string m_imagePath;
	};
}