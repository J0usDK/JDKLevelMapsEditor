#pragma once
#include <vector>
#include <memory>

#include <CryCore/BaseTypes.h>
#include <CryMath/Cry_Vector3.h>

#include "Shared/MapLayers.h"

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	class CFootprintMask;
}

namespace JDKLevelMaps::JDKEditorSource
{
	struct SVegetationInstanceData
	{
		Vec3 pos{ 0, 0, 0 };
		float scale = 1.0f;
		float angle = 0.0f;

		uint16 groupIndex = 0;
		MapLayers::EVegetationLayers layer = MapLayers::EVegetationLayers::Unknown;

		SVegetationInstanceData(Vec3 pos, uint16 groupIndex, float scale, float angle, MapLayers::EVegetationLayers layer) noexcept
			: pos(pos), scale(scale), angle(angle), groupIndex(groupIndex), layer(layer) {
		}
	};

	struct SVegetationQueryResult
	{
		std::vector<SVegetationInstanceData> instances;
		std::vector<std::shared_ptr<const JDKEditorSource::Geometry::CFootprintMask>> footprints;
	};
}