#pragma once
#include <CryMath/Cry_Vector3.h>

#include "Core/Bakers/Vegetation/VegetationData.h"

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	[[nodiscard]] SPlanTransform CalculateInstancePlanTransform(const Vec3& pos, float scale, float angleZ, float angleX, float angleY, float alignToTerrain);

	[[nodiscard]] float QuantizeEditorAngle(float radians) noexcept;
}