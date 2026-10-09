#pragma once
#include <vector>

#include "Shared/MapLayers.h"

namespace JDKLevelMaps::JDKEditorSource
{
	struct SGroupVegParams;
	struct SVegetationInstanceData;

	namespace Internal
	{
		bool CanGenerateProcVeg() noexcept;
		void GenerateProcVeg(const std::vector<MapLayers::EVegetationLayers>& idxToLayer, const std::vector<SGroupVegParams>& idxToGroupParams, bool bSimulateSize, std::vector<SVegetationInstanceData>& result);
	}
}