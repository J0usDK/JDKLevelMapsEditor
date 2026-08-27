#pragma once
#include "VegetationBakerSettings.h"

namespace JDKLevelMaps::Settings
{
	struct SBakerSettings
	{
		float cellSize = 1.0f;
		uint32 tileSize = 64;
		bool bGenerateDebugImage = false;

		SVegetationBakerSettings vegSettings;
	};
}