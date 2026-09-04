#pragma once
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps::Data
{
	struct SRunResult;
}

namespace JDKLevelMaps::ImageWork
{
	class IPNGDataSource
	{
	public:
		virtual ~IPNGDataSource() = default;

		virtual bool FetchRowRGB(uint32 y, uint8* pOutRowRgb) noexcept = 0;
		[[nodiscard]] virtual bool OnProgress(uint32 processedLine) noexcept = 0;
	};

	[[nodiscard]] Data::SRunResult WritePNG(FILE* pFile, uint32 width, uint32 height, IPNGDataSource& dataSource);
}