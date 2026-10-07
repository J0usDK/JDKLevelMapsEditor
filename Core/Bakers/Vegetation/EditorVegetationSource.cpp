#include "StdAfx.h"
#include "EditorVegetationSource.h"

#include <IEditorImpl.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Vegetation/VegetationMap.h>
#include <Vegetation/VegetationObject.h>

#include "ProcVegGenerator.h"
#include "VegetationClassifier.h"
#include "Core/Bakers/Footprint/FootprintMask.h"
#include "Core/Bakers/Footprint/FootprintBuilder.h"
#include "Utils/VectorUtils.h"
#include "Utils/Logger.h"

namespace JDKLevelMaps::JDKEditorSource
{
	namespace
	{
		constexpr float kQueryScaleSlack = 4.0f;

		bool s_bLayoutValid = false;
	}

	namespace
	{
		[[nodiscard]] float ComputeQueryMargin(const std::vector<MapLayers::EVegetationLayers>& idxToLayer)
		{
			float maxRadius = 0.0f;
			for (size_t i = 0; i < idxToLayer.size(); ++i)
			{
				IStatInstGroup group;
				if (idxToLayer[i] != MapLayers::EVegetationLayers::Unknown && gEnv->p3DEngine->GetStatInstGroup(static_cast<int>(i), group))
					maxRadius = std::max(maxRadius, Geometry::CalculateHorizontalRadius(group.pStatObj));
			}

			return maxRadius * kQueryScaleSlack;
		}

		void BuildFootprints(const std::vector<MapLayers::EVegetationLayers>& idxToLayer, const Settings::SVegetationBakerSettings& settings, SVegetationQueryResult& result)
		{
			if (!Utils::Common::TryResize(result.footprints, idxToLayer.size()))
				return;

			std::vector<uint8> used;
			if (!Utils::Common::TryAssign(used, idxToLayer.size(), 0))
				return;

			for (const auto& instance : result.instances)
				used[instance.groupIndex] = 1;

			const auto buildMethod = static_cast<Geometry::EFootprintBuildMethod>(settings.sizeMethod);

			for (size_t i = 0; i < idxToLayer.size(); ++i)
			{
				if (!used[i])
					continue;

				IStatInstGroup group;
				if (gEnv->p3DEngine->GetStatInstGroup(static_cast<int>(i), group) && group.pStatObj)
					result.footprints[i] = Geometry::BuildMaskFromStatObj(group.pStatObj, buildMethod);

				if (!result.footprints[i])
					JDK_WARN("Vegetation group %d has no usable geometry for size simulation. A single cell will be used.", static_cast<int>(i));
			}
		}

		[[nodiscard]] std::vector<MapLayers::EVegetationLayers> BuildGroupLookup(CVegetationMap* pVegetationMap, const Settings::SVegetationBakerSettings& settings, bool& outbProcVecUsed)
		{
			std::vector<MapLayers::EVegetationLayers> result;

#pragma push_macro("GetObject")
#undef GetObject
			const int nCount = pVegetationMap->GetObjectCount();

			int nMaxId = -1;
			for (int i = 0; i < nCount; i++)
				if (CVegetationObject* pObj = pVegetationMap->GetObject(i))
					nMaxId = std::max(nMaxId, pObj->GetId());

			if (nMaxId < 0)
				return {};

			if (!Utils::Common::TryAssign(result, static_cast<size_t>(nMaxId) + 1, MapLayers::EVegetationLayers::Unknown))
				return {};

			std::vector<string> terrainLayers;
			for (int i = 0; i < nCount; i++)
			{
				CVegetationObject* pObj = pVegetationMap->GetObject(i);
				if (!pObj)
					continue;

				const int idx = pObj->GetId();
				if (idx < 0 || idx >= static_cast<int>(result.size()))
					continue;

				const auto layer = Categories::Vegetation::ClassifyGroup(pObj->GetGroup(), settings);
				const bool bIsEnabled = Categories::Vegetation::IsLayerEnabled(layer, settings);

				if (bIsEnabled && !outbProcVecUsed)
				{
					terrainLayers.clear();
					pObj->GetTerrainLayers(terrainLayers);
					if (!terrainLayers.empty())
						outbProcVecUsed = true;
				}

				result[idx] = bIsEnabled ? layer : MapLayers::EVegetationLayers::Unknown;
			}
#pragma pop_macro("GetObject")

			return result;
		}

		void CollectStaticVegetation(CVegetationMap& vegMap, const std::vector<MapLayers::EVegetationLayers>& idxToLayer, const Data::SBounds& bounds, bool bSimulateSize, std::vector<SVegetationInstanceData>& result)
		{
			const float margin = bSimulateSize ? ComputeQueryMargin(idxToLayer) : 0.0f;

			std::vector<CVegetationInstance*> instances;
			vegMap.GetObjectInstances(bounds.fMinX - margin, bounds.fMinY - margin, bounds.fMaxX + margin, bounds.fMaxY + margin, instances);
		
			if (!Utils::Common::TryReserve(result, instances.size()))
				return;

			for (auto* pInstance : instances)
			{
				if (!pInstance || !pInstance->object)
					continue;

				const int idx = pInstance->object->GetId();
				if (idx < 0 || idx >= static_cast<int>(idxToLayer.size()))
					continue;

				const auto layer = idxToLayer[idx];
				if (layer != MapLayers::EVegetationLayers::Unknown)
					result.emplace_back(pInstance->pos, static_cast<uint16>(idx), pInstance->scale, pInstance->angle, layer);
			}
		}
	}

	SVegetationQueryResult QueryVegetationInstances(const Data::SBounds& bounds, const Settings::SVegetationBakerSettings& settings)
	{
		SVegetationQueryResult result;

		CEditorImpl* pEditorImpl = static_cast<CEditorImpl*>(GetIEditor());
		if (!pEditorImpl)
			return SVegetationQueryResult();

		CVegetationMap* pVegetationMap = pEditorImpl->GetVegetationMap();
		if (!pVegetationMap)
			return SVegetationQueryResult();

		bool bProcVegUsed = false;
		const auto& idxToLayer = BuildGroupLookup(pVegetationMap, settings, bProcVegUsed);

		CollectStaticVegetation(*pVegetationMap, idxToLayer, bounds, settings.bSimulateSize, result.instances);

		if (bProcVegUsed)
		{
			if (s_bLayoutValid || (s_bLayoutValid = Internal::CanGenerateProcVeg()))
				Internal::GenerateProcVeg(idxToLayer, result.instances);
			else
				JDK_WARN("Found layout discrepancies. Procedural Vegetation has been disabled.\n"
					"Check the engine version and update the mocked structures and classes.");
		}

		if (settings.bSimulateSize)
			BuildFootprints(idxToLayer, settings, result);

		return result;
	}
}