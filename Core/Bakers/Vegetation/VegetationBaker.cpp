#include "StdAfx.h"
#include "VegetationBaker.h"

#include "EditorVegetationSource.h"
#include "Core/Data/LevelContext.h"
#include "Settings/VegetationBakerSettings.h"
#include "Utils/VectorUtils.h"
#include "Shared/MapHeader.h"

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
}

namespace JDKLevelMaps::Bakers
{
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

		const auto objects = JDKEditorSource::QueryVegetationInstances(
			context.originX, context.originY,
			context.originX + (context.gridWidth * context.cellSize),
			context.originY + (context.gridHeight * context.cellSize),
			m_settings);

		uint8 layerToChannel[3]{ 0xFF, 0xFF, 0xFF };

		uint8 currentChannel = 0;
		if (m_settings.bEnableTree)
			layerToChannel[0] = currentChannel++;
		if (m_settings.bEnableGrass)
			layerToChannel[1] = currentChannel++;
		if (m_settings.bEnableBush)
			layerToChannel[2] = currentChannel++;

		const size_t rowStride = static_cast<size_t>(context.gridWidth) * numChannels;
		const float invCellSize = 1.0f / context.cellSize;
		const float	fGridWidth = static_cast<float>(context.gridWidth);
		const float fGridHeight = static_cast<float>(context.gridHeight);

		for (const auto& object : objects)
		{
			const int32 logicalChannel = MapLayers::ToChannelIndex(object.layer);
			if (logicalChannel < 0)
				continue;

			const uint8 packedChannel = layerToChannel[logicalChannel];
			if (packedChannel == 0xFF)
				continue;

			const float relX = (object.pos.x - context.originX) * invCellSize;
			const float relY = (object.pos.y - context.originY) * invCellSize;

			if (relX < 0.0f || relX >= fGridWidth || relY < 0.0f || relY >= fGridHeight)
				continue;

			const uint32 gridX = static_cast<uint32>(relX);
			const uint32 gridY = static_cast<uint32>(relY);

			const size_t index = static_cast<size_t>(gridY) * rowStride + static_cast<size_t>(gridX) * numChannels + packedChannel;
			
			const uint32 currentDensity = mapData[index] + m_settings.densityPerInstance;
			mapData[index] = static_cast<uint8>(currentDensity > 255 ? 255 : currentDensity);
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
		uint8 mask = GetActiveLayersMask();

		uint32 count = 0;
		for (; mask; mask >>= 1)
			count += (mask & 1);

		return count;
	}

	Bakers::SDebugColor CVegetationBaker::GetDebugColor(uint8 channelsMask, const uint8* pCellData) const noexcept
	{
		return VegetationDebugColorMapper(channelsMask, pCellData);
	}

	const char* CVegetationBaker::GetID() const noexcept { return "VegetationDensity"; }
	EMapType CVegetationBaker::GetMapType() const noexcept { return EMapType::VegetationDensity; }
	Bakers::DebugColorMapperPtr CVegetationBaker::GetDebugColorMapper() const noexcept { return VegetationDebugColorMapper; }
}