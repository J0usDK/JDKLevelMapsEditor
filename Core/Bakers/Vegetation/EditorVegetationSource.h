#pragma once
#include <vector>
#include <CryMath/Cry_Vector3.h>

#include "Shared/MapLayers.h"

class CVegetationObject;
class CVegetationMap;

namespace JDKLevelMaps::Settings
{
	struct SVegetationBakerSettings;
}

namespace JDKLevelMaps::JDKEditorSource::Internal
{
	class CTerrainNode_Mock;
}

namespace JDKLevelMaps::JDKEditorSource
{
	struct SVegetationInstanceData
	{
		Vec3 pos{ 0, 0, 0 };
		MapLayers::EVegetationLayers layer = MapLayers::EVegetationLayers::Unknown;

		SVegetationInstanceData(Vec3 pos, MapLayers::EVegetationLayers layer) noexcept : pos(pos), layer(layer) {}
	};

	class CEditorVegetationSource final
	{
	public:
		[[nodiscard]] static std::vector<SVegetationInstanceData> QueryVegetationInstances(float x1, float y1, float x2, float y2, const Settings::SVegetationBakerSettings& settings);

	private:
		static void CollectProcVegetation(const std::vector<MapLayers::EVegetationLayers>& idxToLayer, std::vector<SVegetationInstanceData>& result);
		static void ProcessTerrainNode(const Internal::CTerrainNode_Mock* pNode, const std::vector<MapLayers::EVegetationLayers>& indexToLayer, std::vector<SVegetationInstanceData>& result);
		[[nodiscard]] static float GetSurfaceTypeAmountCustom(const Internal::CTerrainNode_Mock* pNode, const Vec3& vPos, uint8 ucGlobalSurfType);

		[[nodiscard]] static const std::vector<MapLayers::EVegetationLayers> BuildGroupLookup(CVegetationMap* pVegetationMap, const Settings::SVegetationBakerSettings& settings);
	};
}