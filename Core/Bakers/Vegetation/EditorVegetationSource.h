#pragma once
#include <memory>
#include <vector>

#include <CryMath/Cry_Vector3.h>

#include "VegetationData.h"
#include "Core/Data/Common.h"
#include "Shared/MapLayers.h"

namespace JDKLevelMaps::Settings
{
	struct SVegetationBakerSettings;
}

namespace JDKLevelMaps::JDKEditorSource
{
	[[nodiscard]] SVegetationQueryResult QueryVegetationInstances(const Data::SBounds& bounds, const Settings::SVegetationBakerSettings& settings);
}