#pragma once
#include <vector>

#include <CryCore/BaseTypes.h>

#include "Core/Bakers/IMapBaker.h"
#include "Shared/MapHeader.h"
#include "Utils/ImageView.h"

namespace JDKLevelMaps::ImageWork::Converters
{
	struct SConvertContext
	{
		SMapHeader header;
		uint32 channels;

		SImageView& outImage;
		Bakers::DebugColorMapperPtr colorMapper = nullptr;

		SConvertContext(SMapHeader header, uint32 channels, SImageView& image, Bakers::DebugColorMapperPtr colorMapper)
			: header(header), channels(channels), outImage(image),colorMapper(colorMapper) {}
	};

	inline void MapTileToImage(const std::vector<uint8>& tileData, uint32 tx, uint32 ty, const SConvertContext& ctx)
	{
		const uint32 safeGridHeight = static_cast<uint32>(std::max(0, ctx.header.gridHeight));
		const uint32 safeGridWidth = static_cast<uint32>(std::max(0, ctx.header.gridWidth));
		const uint32 maxLy = std::min(ctx.header.tileSize, static_cast<uint32>(safeGridHeight - (ty * ctx.header.tileSize)));
		const uint32 maxLx = std::min(ctx.header.tileSize, static_cast<uint32>(safeGridWidth - (tx * ctx.header.tileSize)));

		const size_t localStride = static_cast<size_t>(ctx.header.tileSize) * ctx.channels;
		size_t localRowStart = 0;

		const uint32 startGx = tx * ctx.header.tileSize;
		const uint32 startGy = ty * ctx.header.tileSize;

		for (uint32 ly = 0; ly < maxLy; ++ly)
		{
			uchar* pLine = ctx.outImage.ScanLine(startGy + ly);
			uchar* pPixel = pLine + startGx * 3;

			size_t localIndex = localRowStart;
			for (uint32 lx = 0; lx < maxLx; ++lx)
			{
				const Bakers::SDebugColor color = ctx.colorMapper(&tileData[localIndex]);

				pPixel[0] = color.r;
				pPixel[1] = color.g;
				pPixel[2] = color.b;

				pPixel += 3;
				localIndex += ctx.channels;
			}
			localRowStart += localStride;
		}
	}
}