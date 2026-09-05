#pragma once
#include "VegetationBakerSettings.h"

namespace JDKLevelMaps::Settings
{
	enum class EDirectoryFormat : uint8
	{
		Bitmask = 0,
		Hybrid = 1
	};

	struct SBakerSettings
	{
		float cellSize = 1.0f;
		uint32 tileSize = 64;

		EDirectoryFormat directoryFormat = EDirectoryFormat::Bitmask;
		bool bGenerateDebugImage = false;
	};
}