#pragma once
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps::MapLayers
{
	enum class EVegetationLayers : uint8
	{
		Unknown = 0,
		Tree,
		Grass,
		Bush,

		Count
	};

	[[nodiscard]] inline constexpr int32 ToChannelIndex(EVegetationLayers layer) noexcept
	{
		return (layer > EVegetationLayers::Unknown && layer < EVegetationLayers::Count) ? static_cast<int32>(layer) - 1 : -1;
	}

	inline constexpr uint32 kVegetationChannelCount = static_cast<uint32>(EVegetationLayers::Count) - 1;
}