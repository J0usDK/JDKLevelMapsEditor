#include "StdAfx.h"
#include "EditorVegetationSource.h"

#include <IEditorImpl.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Vegetation/VegetationMap.h>
#include <Vegetation/VegetationObject.h>

#include "ProcVegGenerator.h"
#include "VegetationClassifier.h"
#include "Core/Bakers/Geometry/GroupGeometry.h"
#include "Core/Bakers/Geometry/GeometryBuilder.h"
#include "Core/Bakers/Geometry/InstanceTransform.h"
#include "Utils/VectorUtils.h"
#include "Utils/Logger.h"

namespace JDKLevelMaps::JDKEditorSource
{
	namespace
	{
		constexpr float kQueryScaleSlack = 4.0f;
		constexpr float kMaxExpectedLean = 1.0f;

		bool s_bLayoutValid = false;
	}

	namespace
	{
		template<typename T>
		bool TryReadVar(const CVarObject* pVars, const char* name, T& out)
		{
			if (!pVars)
				return false;

			IVariable* pVar = pVars->FindVariable(name);
			if (!pVar)
				return false;

			pVar->Get(out);
			return true;
		}

		[[nodiscard]] float ComputeQueryMargin(const std::vector<MapLayers::EVegetationLayers>& idxToLayer)
		{
			float maxReach = 0.0f;
			for (size_t i = 0; i < idxToLayer.size(); ++i)
			{
				IStatInstGroup group;
				if (idxToLayer[i] != MapLayers::EVegetationLayers::Unknown && gEnv->p3DEngine->GetStatInstGroup(static_cast<int>(i), group) && group.pStatObj)
				{
					const float radius = Geometry::CalculateHorizontalRadius(group.pStatObj);
					const float height = Geometry::CalculateMaxAbsHeight(group.pStatObj);
					maxReach = std::max(maxReach, radius + height * kMaxExpectedLean);
				}
			}

			return maxReach * kQueryScaleSlack;
		}

		void BuildGeometries(const std::vector<MapLayers::EVegetationLayers>& idxToLayer, const Settings::SVegetationBakerSettings& settings, SVegetationQueryResult& result)
		{
			if (!Utils::Common::TryResize(result.geometries, idxToLayer.size()))
				return;

			std::vector<uint8> used;
			if (!Utils::Common::TryAssign(used, idxToLayer.size(), 0))
				return;

			for (const auto& instance : result.instances)
				used[instance.groupIndex] = 1;

			const auto buildMethod = static_cast<Geometry::EGeometryBuildMethod>(settings.sizeMethod);

			for (size_t i = 0; i < idxToLayer.size(); ++i)
			{
				if (!used[i])
					continue;

				IStatInstGroup group;
				if (gEnv->p3DEngine->GetStatInstGroup(static_cast<int>(i), group) && group.pStatObj)
					result.geometries[i] = Geometry::BuildGeometryFromStatObj(group.pStatObj, buildMethod);

				if (!result.geometries[i])
					JDK_WARN("Vegetation group %d has no usable geometry for size simulation. A single cell will be used.", static_cast<int>(i));
			}
		}

		[[nodiscard]] bool BuildGroupData(CVegetationMap* pVegetationMap, const Settings::SVegetationBakerSettings& settings, std::vector<MapLayers::EVegetationLayers>& outIdxToLayer, std::vector<SGroupVegParams>& outIdxToGroupParams, bool& outbProcVecUsed)
		{
#pragma push_macro("GetObject")
#undef GetObject
			const int nCount = pVegetationMap->GetObjectCount();

			int nMaxId = -1;
			for (int i = 0; i < nCount; i++)
				if (CVegetationObject* pObj = pVegetationMap->GetObject(i))
					nMaxId = std::max(nMaxId, pObj->GetId());

			if (nMaxId < 0)
				return false;

			if (!Utils::Common::TryAssign(outIdxToLayer, static_cast<size_t>(nMaxId) + 1, MapLayers::EVegetationLayers::Unknown))
				return false;
			if (!Utils::Common::TryResize(outIdxToGroupParams, static_cast<size_t>(nMaxId) + 1))
				return false;

			std::vector<string> terrainLayers;
			for (int i = 0; i < nCount; i++)
			{
				CVegetationObject* pObj = pVegetationMap->GetObject(i);
				if (!pObj)
					continue;

				const int idx = pObj->GetId();
				if (idx < 0 || idx >= static_cast<int>(outIdxToLayer.size()))
					continue;

				const auto layer = Categories::Vegetation::ClassifyGroup(pObj->GetGroup(), settings);
				const bool bIsEnabled = Categories::Vegetation::IsLayerEnabled(layer, settings);

				if (bIsEnabled)
				{
					SGroupVegParams& params = outIdxToGroupParams[idx];
					const CVarObject* pVars = pObj->GetVarObject();

					float alignPercent = 0.0f;
					if (TryReadVar(pVars, "AlignToTerrainNormals", alignPercent))
						params.alignToTerrain = alignPercent / 100.0f;

					TryReadVar(pVars, "RotationRangeToTerrainNormal", params.rotationRange);
					TryReadVar(pVars, "RandomRotation", params.bRandomRotation);

					if (!outbProcVecUsed)
					{
						terrainLayers.clear();
						pObj->GetTerrainLayers(terrainLayers);
						if (!terrainLayers.empty())
							outbProcVecUsed = true;
					}
				}

				outIdxToLayer[idx] = bIsEnabled ? layer : MapLayers::EVegetationLayers::Unknown;
			}
#pragma pop_macro("GetObject")

			return true;
		}

		void CollectStaticVegetation(CVegetationMap& vegMap, const std::vector<MapLayers::EVegetationLayers>& idxToLayer, const std::vector<SGroupVegParams>& idxToGroupParams, const Data::SBounds& bounds, bool bSimulateSize, std::vector<SVegetationInstanceData>& result)
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
				if (layer == MapLayers::EVegetationLayers::Unknown)
					continue;

				SPlanTransform plan;
				if (bSimulateSize)
				{
					plan = Geometry::CalculateInstancePlanTransform(
						pInstance->pos, pInstance->scale,
						Geometry::QuantizeEditorAngle(pInstance->angle),
						Geometry::QuantizeEditorAngle(pInstance->angleX),
						Geometry::QuantizeEditorAngle(pInstance->angleY),
						idxToGroupParams[idx].alignToTerrain);
				}

				result.emplace_back(pInstance->pos, static_cast<uint16>(idx), plan, layer);
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
		std::vector<SGroupVegParams> idxToGroupParams;
		std::vector<MapLayers::EVegetationLayers> idxToLayer;

		if (!BuildGroupData(pVegetationMap, settings, idxToLayer, idxToGroupParams, bProcVegUsed))
			return SVegetationQueryResult();

		CollectStaticVegetation(*pVegetationMap, idxToLayer, idxToGroupParams, bounds, settings.bSimulateSize, result.instances);

		if (bProcVegUsed)
		{
			if (s_bLayoutValid || (s_bLayoutValid = Internal::CanGenerateProcVeg()))
				Internal::GenerateProcVeg(idxToLayer, idxToGroupParams, settings.bSimulateSize, result.instances);
			else
				JDK_WARN("Found layout discrepancies. Procedural Vegetation has been disabled.\n"
					"Check the engine version and update the mocked structures and classes.");
		}

		if (settings.bSimulateSize)
			BuildGeometries(idxToLayer, settings, result);

		return result;
	}
}