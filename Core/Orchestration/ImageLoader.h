#pragma once
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps
{
	enum class EMapType : uint8;
}

namespace JDKLevelMaps::FileSystem
{
	class CPathResolver;
}

namespace JDKLevelMaps::Data
{
	struct SRunResult;
}

namespace JDKLevelMaps::MapWork
{
	class CMapFileReader;
}

namespace JDKLevelMaps::ImageWork
{
	struct SImageView;
}

namespace JDKLevelMaps::Utils::Common
{
	struct SProgress;
}

namespace JDKLevelMaps::Managers
{
	class CBakersRegistry;

	class CImageLoader final
	{
	public:
		CImageLoader(CBakersRegistry& bakersRegistry, FileSystem::CPathResolver& pathResolver) noexcept;

		[[nodiscard]] Data::SRunResult LoadPreviewFromMap(EMapType bakerType, Utils::Common::SProgress& progress, ImageWork::SImageView& outImage);

		[[nodiscard]] Data::SRunResult LoadPreviewFromDisk(EMapType bakerType, Utils::Common::SProgress& progress, ImageWork::SImageView& outImage);

	private:
		CBakersRegistry& m_bakersRegistry;
		FileSystem::CPathResolver& m_pathResolver;
	};
}