#pragma once
#include <string>
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps::Settings
{
	enum class ESimulateSizeMethod : uint8
	{
		Base = 0,
		Canopy
	};

	struct SVegetationBakerSettings
	{
		uint8 densityPerInstance = 20;

		bool bSimulateSize = false;
		ESimulateSizeMethod sizeMethod = ESimulateSizeMethod::Base;

		bool bEnableGrass = true;
		bool bEnableBush = true;
		bool bEnableTree = true;

		std::string grassGroupName = "grass";
		std::string bushGroupName = "bushes";
		std::string treeGroupName = "trees";
	};
}