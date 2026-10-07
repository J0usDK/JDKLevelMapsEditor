#pragma once
#include <vector>

#include "Shared/MapLayers.h"

namespace JDKLevelMaps::JDKEditorSource
{
	struct SVegetationInstanceData;

	namespace Internal
	{
		bool CanGenerateProcVeg() noexcept;
		void GenerateProcVeg(const std::vector<MapLayers::EVegetationLayers>& idxToLayer, std::vector<SVegetationInstanceData>& result);
	}
}