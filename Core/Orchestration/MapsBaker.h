#pragma once
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps
{
	enum class EMapType : uint8;
	enum class ECompressionAlg : uint8;
	enum class ETileEntryFormat : uint8;
}

namespace JDKLevelMaps::FileSystem
{
	class CPathResolver;
}

namespace JDKLevelMaps::Settings
{
	struct SBakerSettings;
	enum class ECompression : uint8;
	enum class EDirectoryFormat : uint8;
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
	struct SMapFormats;
	class CBakersRegistry;

	class CMapsBaker final
	{
	public:
		CMapsBaker(CBakersRegistry& bakersRegistry, FileSystem::CPathResolver& pathResolver, const Settings::SBakerSettings& bakerSettings) noexcept;

		[[nodiscard]] Data::SRunResult RunBake(EMapType mapType, Utils::Common::SProgress& progress);

	private:
		static ECompressionAlg ResolveCompressionAlg(Settings::ECompression compression) noexcept;
		static uint8 ResolveCompressBlocks(const Bakers::IMapBaker& baker, Settings::ECompression comp, uint32 tileSize, uint64 nonEmptyTilesCount) noexcept;
		static ETileEntryFormat ResolveEntryFormat(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, Settings::EDirectoryFormat dirFormat, uint64 nonEmptyTilesCount, uint8 compBlocksMask) noexcept;

		SMapFormats ResolveMapFormats(const Bakers::IMapBaker& baker, const Data::SLevelContext& context) const noexcept;

	private:
		CBakersRegistry& m_bakersRegistry;
		FileSystem::CPathResolver& m_pathResolver;
		const Settings::SBakerSettings& m_bakerSettings;
	};
}