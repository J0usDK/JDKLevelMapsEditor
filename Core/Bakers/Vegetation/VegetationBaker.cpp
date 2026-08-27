#include "StdAfx.h"
#include "VegetationBaker.h"

#include "VegetationClassifier.h"
#include "EditorVegetationSource.h"
#include "Settings/VegetationBakerSettings.h"
#include "Core/Data/LevelContext.h"

namespace
{
	static JDKLevelMaps::Bakers::SDebugColor VegetationDebugColorMapper(const uint8* pCellData) noexcept
	{
		static_assert(JDKLevelMaps::MapLayers::kVegetationChannelCount == 3, "GetDebugColor assumes exactly 3 channels (RGB). Update logic if layers change");
		if (!pCellData)
			return { 0, 0, 0 };

		const auto calcIntensity = [](uint8 val) noexcept -> uint8
		{
			if (val == 0) return 0;
			return static_cast<uint8>(50 + (val * 205) / 255);
		};

		const uint8 r = pCellData[0];
		const uint8 g = pCellData[1];
		const uint8 b = pCellData[2];

		return { calcIntensity(r), calcIntensity(g), calcIntensity(b) };
	}
}

namespace JDKLevelMaps::Bakers
{
	CVegetationBaker::CVegetationBaker(const Settings::SVegetationBakerSettings& settings) noexcept : m_settings(settings) { }

	std::vector<uint8> CVegetationBaker::Bake(const Data::SLevelContext& context) const
	{
		const uint32 numChannels = GetChannelCount();
		const size_t totalBytes = static_cast<size_t>(context.gridWidth) * context.gridHeight * numChannels;
		std::vector<uint8> mapData(totalBytes, 0);

		const auto objects = JDKEditorSource::QueryVegetationInstances(
			context.originX, context.originY,
			context.originX + (context.gridWidth * context.cellSize),
			context.originY + (context.gridHeight * context.cellSize),
			m_settings);

		const size_t rowStride = static_cast<size_t>(context.gridWidth) * numChannels;
		const float invCellSize = 1.0f / context.cellSize;
		const float	fGridWidth = static_cast<float>(context.gridWidth);
		const float fGridHeight = static_cast<float>(context.gridHeight);

		for (const auto& object : objects)
		{
			const float relX = (object.pos.x - context.originX) * invCellSize;
			const float relY = (object.pos.y - context.originY) * invCellSize;

			if (relX < 0.0f || relX >= fGridWidth ||
				relY < 0.0f || relY >= fGridHeight)
				continue;

			const uint32 gridX = static_cast<uint32>(relX);
			const uint32 gridY = static_cast<uint32>(relY);

			const int32 channelOffset = MapLayers::ToChannelIndex(object.layer);
			CRY_ASSERT(channelOffset >= 0, "[JDKLevelMaps] Vegetation layer was not properly resolved into a channel offset");

			const size_t index = static_cast<size_t>(gridY) * rowStride + static_cast<size_t>(gridX) * numChannels + channelOffset;
			mapData[index] = static_cast<uint8>(std::min(mapData[index] + m_settings.densityPerInstance, 255));
		}

		return mapData;
	}

	Bakers::SDebugColor CVegetationBaker::GetDebugColor(const uint8* pCellData) const noexcept
	{
		return VegetationDebugColorMapper(pCellData);
	}

	const char* CVegetationBaker::GetID() const noexcept { return "VegetationDensity"; }
	EMapType CVegetationBaker::GetMapType() const noexcept { return EMapType::VegetationDensity; }
	uint32 CVegetationBaker::GetChannelCount() const noexcept { return MapLayers::kVegetationChannelCount; }
	Bakers::DebugColorMapperPtr CVegetationBaker::GetDebugColorMapper() const noexcept { return VegetationDebugColorMapper; }
}