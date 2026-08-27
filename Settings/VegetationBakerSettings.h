#pragma once
#include <string>
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps::Settings
{
	struct SVegetationBakerSettings
	{
		uint8 densityPerInstance = 20;

		bool bEnableGrass = true;
		bool bEnableBush = true;
		bool bEnableTree = true;

		std::string grassGroupName = "grass";
		std::string bushGroupName = "bushes";
		std::string treeGroupName = "trees";
	};
}