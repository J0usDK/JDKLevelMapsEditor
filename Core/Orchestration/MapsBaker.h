#pragma once
#include <vector>
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps
{
	enum class EMapType : uint8;
}

namespace JDKLevelMaps::FileSystem
{
	class CPathResolver;
}

namespace JDKLevelMaps::Settings
{
	struct SBakerSettings;
}

namespace JDKLevelMaps::Data
{
	struct SLevelContext;
	struct SRunResult;
}

namespace JDKLevelMaps::Bakers
{
	class IMapBaker;
}

namespace JDKLevelMaps::MapWork
{
	class CMapFileWriter;
}

namespace JDKLevelMaps::ImageWork
{
	class CImageExporter;
}

namespace JDKLevelMaps::Utils::Common
{
	struct SProgress;
}

namespace JDKLevelMaps::Managers
{
	class CBakersRegistry;

	class CMapsBaker final
	{
	public:
		CMapsBaker(CBakersRegistry& bakersRegistry, FileSystem::CPathResolver& pathResolver, const Settings::SBakerSettings& bakerSettings) noexcept;

		[[nodiscard]] Data::SRunResult RunBake(EMapType mapType, Utils::Common::SProgress& progress);

	private:
		[[nodiscard]] Data::SRunResult BuildBakingData(const Bakers::IMapBaker* pBaker, const Data::SLevelContext& context, std::vector<uint8>& outData) noexcept;

	private:
		CBakersRegistry& m_bakersRegistry;
		FileSystem::CPathResolver& m_pathResolver;
		const Settings::SBakerSettings& m_bakerSettings;
	};
}