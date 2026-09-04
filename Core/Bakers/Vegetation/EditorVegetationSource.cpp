#include "StdAfx.h"
#include "EditorVegetationSource.h"

#include <IEditorImpl.h>
#include <Vegetation/VegetationMap.h>
#include <Vegetation/VegetationObject.h>

#include "VegetationClassifier.h"
#include "Utils/VectorUtils.h"

namespace JDKLevelMaps::JDKEditorSource
{
	std::vector<SVegetationInstanceData> QueryVegetationInstances(float x1, float y1, float x2, float y2, const Settings::SVegetationBakerSettings& settings)
	{
		std::vector<SVegetationInstanceData> result;

		CEditorImpl* pEditorImpl = static_cast<CEditorImpl*>(GetIEditor());
		if (!pEditorImpl)
			return result;

		CVegetationMap* pVegetationMap = pEditorImpl->GetVegetationMap();
		if (!pVegetationMap)
			return result;

		std::vector<CVegetationInstance*> instances;
		pVegetationMap->GetObjectInstances(x1, y1, x2, y2, instances);

		if (!Utils::Common::TryReserve(result, instances.size()))
			return result;

		std::unordered_map<CVegetationObject*, MapLayers::EVegetationLayers> objCache;

		if (!Utils::Common::TryReserve(objCache, pVegetationMap->GetObjectCount()))
			return result;

		for (auto pInstance : instances)
		{
			if (!pInstance || !pInstance->object)
				continue;

			auto* pVegObject = pInstance->object;
			auto it = objCache.find(pVegObject);

			if (it == objCache.end())
			{
				const auto layer = Categories::Vegetation::ClassifyGroup(pVegObject->GetGroup(), settings);
				const bool bIsEnabled = Categories::Vegetation::IsLayerEnabled(layer, settings);

				const auto finalLayer = bIsEnabled ? layer : MapLayers::EVegetationLayers::Unknown;

				it = objCache.emplace(pVegObject, finalLayer).first;
			}

			if (it->second != MapLayers::EVegetationLayers::Unknown)
				result.emplace_back(pInstance->pos, it->second);
		}
		return result;
	}
}