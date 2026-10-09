#include "StdAfx.h"
#include "VegetationBaker.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <Includes/JDKMath/BitMath.h>

#include "EditorVegetationSource.h"
#include "Core/Data/LevelContext.h"
#include "Core/Bakers/Geometry/GroupGeometry.h"
#include "Settings/VegetationBakerSettings.h"
#include "Utils/VectorUtils.h"
#include "Utils/Logger.h"
#include "Shared/MapHeader.h"

namespace JDKLevelMaps::Bakers
{
	namespace
	{
		constexpr float kMinSplatRadiusCells = 0.5f;

		constexpr float kSubpixelTarget = 0.025f;
		constexpr int kMaxSubdivision = 16;
		constexpr size_t kMaxScratchPixels = 16u * 1024u * 1024u;

		struct SGridRect
		{
			int32 x0 = 0, x1 = -1;
			int32 y0 = 0, y1 = -1;

			[[nodiscard]] bool IsValid() const noexcept { return x0 <= x1 && y0 <= y1; }
		};
	}

	namespace
	{
		JDKLevelMaps::Bakers::SDebugColor VegetationDebugColorMapper(uint8 channelsMask, const uint8* pCellData) noexcept
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

		inline void AddCellDensity(uint8* pData, size_t gridWidth, uint32 channelsCount, uint8 channel, int32 x, int32 y, float weight, uint8 density) noexcept
		{
			const size_t idx = (static_cast<size_t>(y) * gridWidth + static_cast<size_t>(x)) * channelsCount + channel;
			const uint32 added = static_cast<uint32>(density * weight + 0.5f);
			const uint32 sum = pData[idx] + added;

			pData[idx] = static_cast<uint8>(sum > 255 ? 255 : sum);
		}

		inline void AddCellDensityFast(uint8* pData, size_t rowOffset, uint32 channelsCount, int32 x, uint32 addedDensity)
		{
			const size_t idx = rowOffset + static_cast<size_t>(x) * channelsCount;
			const uint32 sum = pData[idx] + addedDensity;

			pData[idx] = static_cast<uint8>(std::min(255u, sum));
		}

		inline void SplatSinglePoint(const Data::SLevelContext& ctx, uint8* pData, uint32 channelsCount, uint8 channel, uint8 density, const Vec3& pos) noexcept
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

		void RasterizeTriangle(const Vec2& a, const Vec2& b, const Vec2& c, float ox, float oy, float invSub, int W, int H, uint8* pBits) noexcept
		{
			const float xs[3] = { (a.x - ox) * invSub, (b.x - ox) * invSub, (c.x - ox) * invSub };
			const float ys[3] = { (a.y - oy) * invSub, (b.y - oy) * invSub, (c.y - oy) * invSub };

			const float minY = std::min(ys[0], std::min(ys[1], ys[2]));
			const float maxY = std::max(ys[0], std::max(ys[1], ys[2]));

			if (maxY < 0.0f || minY > static_cast<float>(H))
				return;

			const int y0 = std::max(0, static_cast<int>(std::ceil(minY - 0.5f)));
			const int y1 = std::min(H - 1, static_cast<int>(std::floor(maxY - 0.5f)));

			for (int y = y0; y <= y1; ++y)
			{
				const float py = static_cast<float>(y) + 0.5f;

				float xl = std::numeric_limits<float>::max();
				float xr = std::numeric_limits<float>::lowest();

				for (int e = 0; e < 3; ++e)
				{
					const int i = e;
					const int j = (e + 1) % 3;

					if ((py < ys[i]) == (py < ys[j]))
						continue;

					const float x = xs[i] + (py - ys[i]) / (ys[j] - ys[i]) * (xs[j] - xs[i]);
					xl = std::min(xl, x);
					xr = std::max(xr, x);
				}

				if (xl > xr)
					continue;

				const int x0 = std::max(0, static_cast<int>(std::ceil(xl - 0.5f)));
				const int x1 = std::min(W - 1, static_cast<int>(std::floor(xr - 0.5f)));

				if (x0 <= x1)
					std::memset(pBits + static_cast<size_t>(y) * W + x0, 1, static_cast<size_t>(x1 - x0 + 1));
			}
		}

		bool TransformAndBoundGeometry(const JDKEditorSource::Geometry::CGroupGeometry& geometry, const Vec3& pos, const JDKEditorSource::SPlanTransform& plan, std::vector<Vec2>& outProjected, Data::SBounds& outBounds) noexcept
		{
			const auto& tris = geometry.GetTriangles();
			if (tris.empty())
				return false;
			if (!Utils::Common::TryResize(outProjected, tris.size() * 3))
				return false;

			float minX = std::numeric_limits<float>::max();
			float maxX = std::numeric_limits<float>::lowest();
			float minY = std::numeric_limits<float>::max();
			float maxY = std::numeric_limits<float>::lowest();

			size_t n = 0;
			for (const auto& tri : tris)
			{
				for (const Vec3& v : tri.v)
				{
					const float wx = pos.x + plan.a00 * v.x + plan.a01 * v.y + plan.leanX * v.z;
					const float wy = pos.y + plan.a10 * v.x + plan.a11 * v.y + plan.leanY * v.z;

					outProjected[n++] = Vec2(wx, wy);

					minX = std::min(minX, wx);
					maxX = std::max(maxX, wx);
					minY = std::min(minY, wy);
					maxY = std::max(maxY, wy);
				}
			}

			outBounds = Data::SBounds(minX, maxX, minY, maxY);
			return true;
		}

		bool RasterizeProjected(const std::vector<Vec2>& projected, const SGridRect& rect, const Data::SLevelContext& ctx, bool bFillEnclosed, int& outS, int& outW, int& outH, std::vector<uint8>& outBits)
		{
			const int cellsW = rect.x1 - rect.x0 + 1;
			const int cellsH = rect.y1 - rect.y0 + 1;

			outS = std::clamp(static_cast<int>(std::ceil(ctx.cellSize / kSubpixelTarget)), 1, kMaxSubdivision);
			while (outS > 1 && static_cast<size_t>(cellsW) * outS * static_cast<size_t>(cellsH) * outS > kMaxScratchPixels)
				--outS;

			outW = cellsW * outS;
			outH = cellsH * outS;

			if (static_cast<size_t>(outW) * static_cast<size_t>(outH) > kMaxScratchPixels)
				return false;
			if (!Utils::Common::TryAssign(outBits, static_cast<size_t>(outW) * static_cast<size_t>(outH), 0))
				return false;

			const float sub = ctx.cellSize / static_cast<float>(outS);
			const float invSub = 1.0f / sub;
			const float ox = ctx.originX + static_cast<float>(rect.x0) * ctx.cellSize;
			const float oy = ctx.originY + static_cast<float>(rect.y0) * ctx.cellSize;

			const size_t n = projected.size();
			for (size_t i = 0; i + 2 < n; i += 3)
				RasterizeTriangle(projected[i], projected[i + 1], projected[i + 2], ox, oy, invSub, outW, outH, outBits.data());

			if (bFillEnclosed)
				JDKEditorSource::Geometry::FillEnclosedAreas(outBits, outW, outH);

			return true;
		}

		void DownsampleToGrid(const std::vector<uint8>& bits, int S, int W, int cellsW, int cellsH, const SGridRect& rect, const Data::SLevelContext& ctx, uint8* pData, uint32 channelsCount, uint8 channel, uint8 density, const Vec3& pos) noexcept
		{
			const int32 homeX = static_cast<int32>(std::floor((pos.x - ctx.originX) * ctx.invCellSize));
			const int32 homeY = static_cast<int32>(std::floor((pos.y - ctx.originY) * ctx.invCellSize));
			const bool bHomeInGrid = homeX >= 0 && homeX < ctx.gridWidth && homeY >= 0 && homeY < ctx.gridHeight;

			if (bHomeInGrid)
				AddCellDensity(pData, ctx.gridWidth, channelsCount, channel, homeX, homeY, 1.0f, density);

			const float invArea = 1.0f / static_cast<float>(S * S);
			const size_t stride = static_cast<size_t>(ctx.gridWidth) * channelsCount;

			for (int cy = 0; cy < cellsH; ++cy)
			{
				const int32 y = rect.y0 + cy;
				const size_t rowOffset = static_cast<size_t>(y) * stride + channel;

				for (int cx = 0; cx < cellsW; ++cx)
				{
					const int32 x = rect.x0 + cx;
					if (bHomeInGrid && x == homeX && y == homeY)
						continue;

					const size_t baseOffset = static_cast<size_t>(cy * S) * W + static_cast<size_t>(cx) * S;
					uint32 count = 0;
					for (int sy = 0; sy < S; ++sy)
					{
						const uint8* pRow = &bits[baseOffset + static_cast<size_t>(sy) * W];
						for (int sx = 0; sx < S; ++sx)
							count += pRow[sx];
					}

					if (count == 0)
						continue;

					const uint32 addedDensity = static_cast<uint32>(static_cast<float>(count) * invArea * density + 0.5f);
					if (addedDensity)
						AddCellDensityFast(pData, rowOffset, channelsCount, x, addedDensity);
				}
			}
		}

		void SplatProjected(const Data::SLevelContext& ctx, uint8* pData, uint32 channelsCount, uint8 channel, uint8 density, const JDKEditorSource::Geometry::CGroupGeometry& geometry, const Vec3& pos, const JDKEditorSource::SPlanTransform& plan)
		{
			thread_local std::vector<Vec2> projected;
			thread_local std::vector<uint8> bits;

			Data::SBounds bounds(0, 0, 0, 0);
			if (!TransformAndBoundGeometry(geometry, pos, plan, projected, bounds))
				return;

			const SGridRect rect = ProjectBoundsToGrid(bounds, ctx);
			if (!rect.IsValid())
				return;

			int S = 0, W = 0, H = 0;
			if (!RasterizeProjected(projected, rect, ctx, geometry.ShouldFillEnclosed(), S, W, H, bits))
				return;

			const int cellsW = rect.x1 - rect.x0 + 1;
			const int cellsH = rect.y1 - rect.y0 + 1;
			DownsampleToGrid(bits, S, W, cellsW, cellsH, rect, ctx, pData, channelsCount, channel, density, pos);
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

			bool bSplatAsGeometry = false;

			if (m_settings.bSimulateSize && object.groupIndex < query.geometries.size())
			{
				const JDKEditorSource::Geometry::CGroupGeometry* pGeometry = query.geometries[object.groupIndex].get();
				if (pGeometry)
				{
					float leanImpact = 0.0f;
					if (object.plan.leanX != 0.0f || object.plan.leanY != 0.0f)
						leanImpact = std::hypot(object.plan.leanX, object.plan.leanY) * pGeometry->GetMaxAbsZ();

					const float reach = pGeometry->GetBoundingRadius() * object.plan.GetStretch() + leanImpact;
					if (reach * context.invCellSize > kMinSplatRadiusCells)
					{
						SplatProjected(context, mapData.data(), numChannels, packedChannel, m_settings.densityPerInstance, *pGeometry, object.pos, object.plan);
						bSplatAsGeometry = true;
					}
				}
			}

			if (!bSplatAsGeometry)
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