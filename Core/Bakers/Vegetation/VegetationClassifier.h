#pragma once
#include <string>
#include <CryCore/BaseTypes.h>

#include "Settings/VegetationBakerSettings.h"
#include "Shared/MapLayers.h"

namespace JDKLevelMaps::Categories::Vegetation
{
	[[nodiscard]] inline MapLayers::EVegetationLayers ClassifyGroup(const char* groupName, const Settings::SVegetationBakerSettings& settings) noexcept
	{
		if (!groupName)
			return MapLayers::EVegetationLayers::Unknown;

		if (strcmp(groupName, settings.grassGroupName.c_str()) == 0)
			return MapLayers::EVegetationLayers::Grass;
		if (strcmp(groupName, settings.treeGroupName.c_str()) == 0)
			return MapLayers::EVegetationLayers::Tree;
		if (strcmp(groupName, settings.bushGroupName.c_str()) == 0)
			return MapLayers::EVegetationLayers::Bush;

		return MapLayers::EVegetationLayers::Unknown;
	}

	[[nodiscard]] inline bool IsLayerEnabled(MapLayers::EVegetationLayers layer, const Settings::SVegetationBakerSettings& settings) noexcept
	{
		return (layer == MapLayers::EVegetationLayers::Tree && settings.bEnableTree) ||
			(layer == MapLayers::EVegetationLayers::Bush && settings.bEnableBush) ||
			(layer == MapLayers::EVegetationLayers::Grass && settings.bEnableGrass);
	}
}