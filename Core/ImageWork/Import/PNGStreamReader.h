#pragma once
#include <cstdio>

namespace JDKLevelMaps::Utils::Common
{
	struct SProgressTask;
}

namespace JDKLevelMaps::Data
{
	struct SRunResult;
}

namespace JDKLevelMaps::ImageWork
{
	struct SImageSizes { uint32 width, height; };

	struct SImageView;

	[[nodiscard]] Data::SRunResult ReadPNGInfo(FILE* pFile, SImageSizes& outImageSizes);

	[[nodiscard]] Data::SRunResult ReadPNG(FILE* pFile, SImageSizes& imageSizes, SImageView& outImageView, Utils::Common::SProgressTask* pTask);
}