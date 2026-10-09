#pragma once
#include <vector>
#include <memory>

#include <CryCore/BaseTypes.h>
#include <CryMath/Cry_Vector3.h>

#include "Shared/MapLayers.h"

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	class CGroupGeometry;
}

namespace JDKLevelMaps::JDKEditorSource
{
	struct SPlanTransform
	{
		float a00 = 1.0f, a01 = 0.0f;
		float a10 = 0.0f, a11 = 1.0f;
		float leanX = 0.0f, leanY = 0.0f;

		[[nodiscard]] float GetStretch() const noexcept
		{
			return std::sqrt(std::max(a00 * a00 + a10 * a10, a01 * a01 + a11 * a11));
		}
	};

	struct SGroupVegParams
	{
		float alignToTerrain = 0.0f;
		bool bRandomRotation = false;
		int rotationRange = 0;
	};

	struct SVegetationInstanceData
	{
		Vec3 pos{ 0, 0, 0 };
		SPlanTransform plan;

		uint16 groupIndex = 0;
		MapLayers::EVegetationLayers layer = MapLayers::EVegetationLayers::Unknown;

		SVegetationInstanceData(Vec3 pos, uint16 groupIndex, const SPlanTransform& plan, MapLayers::EVegetationLayers layer) noexcept
			: pos(pos), plan(plan), groupIndex(groupIndex), layer(layer) {
		}
	};

	struct SVegetationQueryResult
	{
		std::vector<SVegetationInstanceData> instances;
		std::vector<std::shared_ptr<const Geometry::CGroupGeometry>> geometries;
	};
}