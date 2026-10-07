#include "StdAfx.h"
#include "VegetationBaker.h"

#include <Includes/JDKMath/BitMath.h>

#include "EditorVegetationSource.h"
#include "Core/Data/LevelContext.h"
#include "Core/Bakers/Footprint/FootprintMask.h"
#include "Settings/VegetationBakerSettings.h"
#include "Utils/VectorUtils.h"
#include "Utils/Logger.h"
#include "Shared/MapHeader.h"

namespace JDKLevelMaps::Bakers
{
	namespace
	{
		constexpr float kMinSplatRadiusCells = 0.5f;

		struct SGridRect
		{
			int32 x0 = 0, x1 = -1;
			int32 y0 = 0, y1 = -1;

			[[nodiscard]] bool IsValid() const noexcept { return x0 <= x1 && y0 <= y1; }
		};
	}

	namespace
	{
		static JDKLevelMaps::Bakers::SDebugColor VegetationDebugColorMapper(uint8 channelsMask, const uint8* pCellData) noexcept
		{
			if (!pCellData)
				return { 0, 0, 0 };

			const auto calcIntensity = [](uint8 val) noexcept -> uint8
			{
				if (val == 0) return 0;
				return static_cast<uint8>(50 + (val * 205) / 255);
			};

			uint8 r = 0, g = 0, b = 0;
			uint32 offset = 0;

			if (channelsMask & (1 << static_cast<uint8>(JDKLevelMaps::MapLayers::EVegetationLayers::Tree)))
				r = pCellData[offset++];
			if (channelsMask & (1 << static_cast<uint8>(JDKLevelMaps::MapLayers::EVegetationLayers::Grass)))
				g = pCellData[offset++];
			if (channelsMask & (1 << static_cast<uint8>(JDKLevelMaps::MapLayers::EVegetationLayers::Bush)))
				b = pCellData[offset++];

			return { calcIntensity(r), calcIntensity(g), calcIntensity(b) };
		}

		void AddCellDensity(uint8* pData, size_t gridWidth, uint32 channelsCount, uint8 channel, int32 x, int32 y, float weight, uint8 density) noexcept
		{
			const size_t idx = (static_cast<size_t>(y) * gridWidth + static_cast<size_t>(x)) * channelsCount + channel;
			const uint32 added = static_cast<uint32>(density * weight + 0.5f);
			const uint32 sum = pData[idx] + added;

			pData[idx] = static_cast<uint8>(sum > 255 ? 255 : sum);
		}

		void AddCellDensityFast(uint8* pData, size_t rowOffset, uint32 channelsCount, int32 x, uint32 addedDensity)
		{
			const size_t idx = rowOffset + static_cast<size_t>(x) * channelsCount;
			const uint32 sum = pData[idx] + addedDensity;

			pData[idx] = static_cast<uint8>(std::min(255u, sum));
		}

		void SplatSinglePoint(const Data::SLevelContext& ctx, uint8* pData, uint32 channelsCount, uint8 channel, uint8 density, const Vec3& pos) noexcept
		{
			const float relX = (pos.x - ctx.originX) * ctx.invCellSize;
			const float relY = (pos.y - ctx.originY) * ctx.invCellSize;

			if (!(relX >= 0.0f && relY >= 0.0f))
				return;

			const int32 x = static_cast<int32>(relX);
			const int32 y = static_cast<int32>(relY);

			if (x >= 0 && x < ctx.gridWidth && y >= 0 && y < ctx.gridHeight)
			{
				const size_t rowOffset = static_cast<size_t>(y) * ctx.gridWidth * channelsCount + channel;
				AddCellDensityFast(pData, rowOffset, channelsCount, x, density);
			}
		}

		[[nodiscard]] Data::SBounds CalculateRotatedBounds(const Data::SBounds& localBounds, const Vec3& pos, float scale, float cosA, float sinA) noexcept
		{
			const float cornersX[2] = { localBounds.fMinX, localBounds.fMaxX };
			const float cornersY[2] = { localBounds.fMinY, localBounds.fMaxY };

			float minX = std::numeric_limits<float>::max();
			float minY = std::numeric_limits<float>::max();
			float maxX = std::numeric_limits<float>::lowest();
			float maxY = std::numeric_limits<float>::lowest();

			for (int i = 0; i < 2; ++i)
			{
				for (int j = 0; j < 2; ++j)
				{
					const float lx = cornersX[i] * scale;
					const float ly = cornersY[j] * scale;
					const float wx = pos.x + lx * cosA - ly * sinA;
					const float wy = pos.y + lx * sinA + ly * cosA;

					minX = std::min(minX, wx);
					maxX = std::max(maxX, wx);
					minY = std::min(minY, wy);
					maxY = std::max(maxY, wy);

				}
			}

			return Data::SBounds(minX, maxX, minY, maxY);
		}

		SGridRect ProjectBoundsToGrid(const Data::SBounds& worldBounds, const Data::SLevelContext& ctx) noexcept
		{
			const float fW = static_cast<float>(ctx.gridWidth);
			const float fH = static_cast<float>(ctx.gridHeight);

			const int32	x0 = static_cast<int32>(std::max(0.0f, std::floor((worldBounds.fMinX - ctx.originX) * ctx.invCellSize)));
			const int32 x1 = static_cast<int32>(std::min(fW - 1.0f, std::floor((worldBounds.fMaxX - ctx.originX) * ctx.invCellSize)));
			const int32 y0 = static_cast<int32>(std::max(0.0f, std::floor((worldBounds.fMinY - ctx.originY) * ctx.invCellSize)));
			const int32 y1 = static_cast<int32>(std::min(fH - 1.0f, std::floor((worldBounds.fMaxY - ctx.originY) * ctx.invCellSize)));

			return SGridRect{ x0, x1, y0, y1 };
		}

		void SplatMask(const Data::SLevelContext& ctx, uint8* pData, uint32 channelsCount, uint8 channel, uint8 density, const JDKEditorSource::Geometry::CFootprintMask& mask, const Vec3& pos, float scale, float angle)
		{
			const float cosA = std::cos(angle);
			const float sinA = std::sin(angle);

			const auto worldBounds = CalculateRotatedBounds(mask.GetBounds(), pos, scale, cosA, sinA);
			const auto rect = ProjectBoundsToGrid(worldBounds, ctx);
			if (!rect.IsValid())
				return;

			const int32 homeX = static_cast<int32>(std::floor((pos.x - ctx.originX) * ctx.invCellSize));
			const int32 homeY = static_cast<int32>(std::floor((pos.y - ctx.originY) * ctx.invCellSize));
			const bool bHomeInGrid = homeX >= 0 && homeX < ctx.gridWidth && homeY >= 0 && homeY < ctx.gridHeight;

			if (bHomeInGrid)
				AddCellDensity(pData, ctx.gridWidth, channelsCount, channel, homeX, homeY, 1.0f, density);

			const float invScale = 1.0f / scale;
			const float halfWindow = 0.5f * ctx.cellSize * invScale;

			const float dLx = cosA * ctx.cellSize * invScale;
			const float dLy = -sinA * ctx.cellSize * invScale;

			const size_t stride = static_cast<size_t>(ctx.gridWidth) * channelsCount;
			const size_t baseOffset = channel;

			for (int32 y = rect.y0; y <= rect.y1; ++y)
			{
				const size_t rowOffset = static_cast<size_t>(y) * stride + baseOffset;

				const float wy = ctx.originY + (static_cast<float>(y) + 0.5f) * ctx.cellSize - pos.y;
				const float wx0 = ctx.originX + (static_cast<float>(rect.x0) + 0.5f) * ctx.cellSize - pos.x;
				float lx = (cosA * wx0 + sinA * wy) * invScale;
				float ly = (-sinA * wx0 + cosA * wy) * invScale;

				for (int32 x = rect.x0; x <= rect.x1; ++x)
				{
					if (x != homeX || y != homeY)
					{
						const float coverage = mask.SampleCoverage({ lx, ly }, halfWindow);
						const uint32 addedDensity = static_cast<uint32>(coverage * density + 0.5f);

						if (addedDensity > 0)
							AddCellDensityFast(pData, rowOffset, channelsCount, x, addedDensity);
					}

					lx += dLx;
					ly += dLy;
				}
			}
		}
	}
	CVegetationBaker::CVegetationBaker(const Settings::SVegetationBakerSettings& settings) noexcept : m_settings(settings) { }

	std::vector<uint8> CVegetationBaker::Bake(const Data::SLevelContext& context) const
	{
		const uint32 numChannels = GetChannelCount();
		if (numChannels == 0)
			return {};

		const size_t totalBytes = static_cast<size_t>(context.gridWidth) * context.gridHeight * numChannels;
		std::vector<uint8> mapData;

		if (!Utils::Common::TryAssign(mapData, totalBytes, 0))
			return {};

		const Data::SBounds bounds(
			context.originX, context.originX + (context.gridWidth * context.cellSize),
			context.originY, context.originY + (context.gridHeight * context.cellSize));

		const auto query = JDKEditorSource::QueryVegetationInstances(bounds, m_settings);

		uint8 layerToChannel[3]{ 0xFF, 0xFF, 0xFF };

		uint8 currentChannel = 0;
		if (m_settings.bEnableTree)
			layerToChannel[0] = currentChannel++;
		if (m_settings.bEnableGrass)
			layerToChannel[1] = currentChannel++;
		if (m_settings.bEnableBush)
			layerToChannel[2] = currentChannel++;

		for (const auto& object : query.instances)
		{
			const int32 logicalChannel = MapLayers::ToChannelIndex(object.layer);
			if (logicalChannel < 0)
				continue;

			const uint8 packedChannel = layerToChannel[logicalChannel];
			if (packedChannel == 0xFF)
				continue;

			bool bSplatAsMask = false;

			if (m_settings.bSimulateSize && object.scale > 0.0f && object.groupIndex < query.footprints.size())
			{
				const JDKEditorSource::Geometry::CFootprintMask* pMask = query.footprints[object.groupIndex].get();
				if (pMask && pMask->GetBoundingRadius() * object.scale * context.invCellSize > kMinSplatRadiusCells)
				{
					SplatMask(context, mapData.data(), numChannels, packedChannel, m_settings.densityPerInstance, *pMask, object.pos, object.scale, object.angle);
					bSplatAsMask = true;
				}
			}

			if (!bSplatAsMask)
				SplatSinglePoint(context, mapData.data(), numChannels, packedChannel, m_settings.densityPerInstance, object.pos);
		}

		return mapData;
	}

	uint8 CVegetationBaker::GetActiveLayersMask() const noexcept
	{
		uint8 mask = 0;

		if (m_settings.bEnableTree)
			mask |= (1 << static_cast<uint8>(MapLayers::EVegetationLayers::Tree));
		if (m_settings.bEnableGrass)
			mask |= (1 << static_cast<uint8>(MapLayers::EVegetationLayers::Grass));
		if (m_settings.bEnableBush)
			mask |= (1 << static_cast<uint8>(MapLayers::EVegetationLayers::Bush));

		return mask;
	}

	uint32 CVegetationBaker::GetChannelCount() const noexcept
	{
		return JDK::Math::BitMath::PopCount8(GetActiveLayersMask());
	}

	Bakers::SDebugColor CVegetationBaker::GetDebugColor(uint8 channelsMask, const uint8* pCellData) const noexcept
	{
		return VegetationDebugColorMapper(channelsMask, pCellData);
	}

	const char* CVegetationBaker::GetID() const noexcept { return "VegetationDensity"; }
	EMapType CVegetationBaker::GetMapType() const noexcept { return EMapType::VegetationDensity; }
	Bakers::DebugColorMapperPtr CVegetationBaker::GetDebugColorMapper() const noexcept { return VegetationDebugColorMapper; }
}