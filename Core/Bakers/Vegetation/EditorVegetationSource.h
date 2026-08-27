#pragma once
#include <vector>
#include <CryMath/Cry_Vector3.h>

#include "Shared/MapLayers.h"

namespace JDKLevelMaps::Settings
{
	struct SVegetationBakerSettings;
}

namespace JDKLevelMaps::JDKEditorSource
{
	struct SVegetationInstanceData
	{
		Vec3 pos{ 0, 0, 0 };
		MapLayers::EVegetationLayers layer = MapLayers::EVegetationLayers::Unknown;

		SVegetationInstanceData(Vec3 pos, MapLayers::EVegetationLayers layer) noexcept : pos(pos), layer(layer) {}
	};

	[[nodiscard]] std::vector<SVegetationInstanceData> QueryVegetationInstances(float x1, float y1, float x2, float y2, const Settings::SVegetationBakerSettings& settings);
}