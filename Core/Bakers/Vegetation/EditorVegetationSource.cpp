#include "StdAfx.h"
#include "EditorVegetationSource.h"

#include <IEditorImpl.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Vegetation/VegetationMap.h>
#include <Vegetation/VegetationObject.h>

#include "MockTerrain.h"
#include "VegetationClassifier.h"
#include "Utils/VectorUtils.h"
#include "Utils/Logger.h"

namespace
{
	bool s_bProcVegUsed = false;
	bool s_bLayoutValid = false;
}

namespace JDKLevelMaps::JDKEditorSource
{
	std::vector<SVegetationInstanceData> CEditorVegetationSource::QueryVegetationInstances(float x1, float y1, float x2, float y2, const Settings::SVegetationBakerSettings& settings)
	{
		std::vector<SVegetationInstanceData> result;

		CEditorImpl* pEditorImpl = static_cast<CEditorImpl*>(GetIEditor());
		if (!pEditorImpl)
			return {};

		CVegetationMap* pVegetationMap = pEditorImpl->GetVegetationMap();
		if (!pVegetationMap)
			return {};

		const auto& indexToLayer = BuildGroupLookup(pVegetationMap, settings);

		std::vector<CVegetationInstance*> instances;
		pVegetationMap->GetObjectInstances(x1, y1, x2, y2, instances);

		if (!Utils::Common::TryReserve(result, instances.size()))
			return {};

		for (auto pInstance : instances)
		{
			if (!pInstance || !pInstance->object)
				continue;

			const int idx = pInstance->object->GetId();

			if (idx < 0 || idx >= (int)indexToLayer.size())
				continue;

			const auto layer = indexToLayer[idx];
			if (layer != MapLayers::EVegetationLayers::Unknown)
				result.emplace_back(pInstance->pos, layer);
		}

		if (s_bProcVegUsed)
		{
			if (!s_bLayoutValid)
				s_bLayoutValid = Internal::ValidateMockLayout();

			if (s_bLayoutValid)
				CollectProcVegetation(indexToLayer, result);
			else
				JDK_WARN("Found layout discrepancies. Procedural Vegetation has been disabled.\n"
					"Check the engine version and update the mocked structures and classes.");
		}

		return result;
	}

	const std::vector<MapLayers::EVegetationLayers> CEditorVegetationSource::BuildGroupLookup(CVegetationMap* pVegetationMap, const Settings::SVegetationBakerSettings& settings)
	{
		s_bProcVegUsed = false;
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

		if (!Utils::Common::TryResize(result, nMaxId + 1))
			return {};

		std::fill(result.begin(), result.end(), MapLayers::EVegetationLayers::Unknown);

		for (int i = 0; i < nCount; i++)
		{
			CVegetationObject* pObj = pVegetationMap->GetObject(i);
			if (!pObj)
				continue;

			const int idx = pObj->GetId();
			if (idx < 0 || idx >= (int)result.size())
				continue;

			const auto layer = Categories::Vegetation::ClassifyGroup(pObj->GetGroup(), settings);
			const bool bIsEnabled = Categories::Vegetation::IsLayerEnabled(layer, settings);

			if (bIsEnabled && !s_bProcVegUsed)
			{
				std::vector<string> terrainLayers;
				pObj->GetTerrainLayers(terrainLayers);
				if (!terrainLayers.empty())
					s_bProcVegUsed = true;
			}

			result[idx] = bIsEnabled ? layer : MapLayers::EVegetationLayers::Unknown;
		}
#pragma pop_macro("GetObject")

		return result;
	}

	void CEditorVegetationSource::CollectProcVegetation(const std::vector<MapLayers::EVegetationLayers>& idxToLayer, std::vector<SVegetationInstanceData>& result)
	{
		ITerrain* pITerrain = gEnv->p3DEngine->GetITerrain();
		if (!pITerrain)
			return;

		const auto* pTerrain = reinterpret_cast<const Internal::CTerrain_Mock*>(pITerrain);
		if (pTerrain->m_arrSecInfoPyramid.m_nCount == 0 || !pTerrain->m_arrSecInfoPyramid.m_pElements)
			return;

		const auto& level0 = pTerrain->m_arrSecInfoPyramid.m_pElements[0];

		if (!level0.m_pData || level0.m_nSize <= 0)
			return;

		int nGridSize = level0.m_nSize;

		for (int i = 0; i < nGridSize * nGridSize; ++i)
		{
			const auto* pNode = level0.m_pData[i];
			if (pNode)
			{
				__try
				{
					ProcessTerrainNode(pNode, idxToLayer, result);
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					JDK_ERR("Access violation while reading terrain node.\n"
						"Check the engine version and update the mocked structures and classes.");
					return;
				}
			}
		}
	}

	void CEditorVegetationSource::ProcessTerrainNode(const Internal::CTerrainNode_Mock* pNode, const std::vector<MapLayers::EVegetationLayers>& idxToLayer, std::vector<SVegetationInstanceData>& result)
	{
		ICVar* pNoRandomSeed = gEnv->pConsole->GetCVar("bNoRandomSeed");
		bool bNoSeed = pNoRandomSeed && pNoRandomSeed->GetIVal() != 0;
		CMTRand_int32 rndGen(bNoSeed ? 0 : pNode->m_nOriginX + pNode->m_nOriginY);

		float fTerrainUnitSize = gEnv->p3DEngine->GetHeightMapUnitSize();
		float fTerrainSize = (float)gEnv->p3DEngine->GetTerrainSize();
		float fSectorSize = (float)(gEnv->p3DEngine->GetTerrainSectorSize() << pNode->m_nTreeLevel);

		float fMinX = (float)pNode->m_nOriginX;
		float fMinY = (float)pNode->m_nOriginY;
		float fMaxX = fMinX + fSectorSize;
		float fMaxY = fMinY + fSectorSize;

		float fProcVegMaxViewDist = gEnv->pConsole->GetCVar("e_ProcVegetationMaxViewDistance") ? gEnv->pConsole->GetCVar("e_ProcVegetationMaxViewDistance")->GetFVal() : 128.0f;
		float fViewDistRatioVeg = gEnv->pConsole->GetCVar("e_ViewDistRatioVegetation") ? gEnv->pConsole->GetCVar("e_ViewDistRatioVegetation")->GetFVal() : 100.0f;
		float fVegMinSize = gEnv->pConsole->GetCVar("e_VegetationMinSize") ? gEnv->pConsole->GetCVar("e_VegetationMinSize")->GetFVal() : 0.0f;

		int nProcVegMaxCacheLevels = gEnv->pConsole->GetCVar("e_ProcVegetationMaxCacheLevels") ? gEnv->pConsole->GetCVar("e_ProcVegetationMaxCacheLevels")->GetIVal() : 1;
		int nProcVeg = gEnv->pConsole->GetCVar("e_ProcVegetation") ? gEnv->pConsole->GetCVar("e_ProcVegetation")->GetIVal() : 1;

		for (int nLayer = 0; nLayer < pNode->m_lstSurfaceTypeInfo.Count(); nLayer++)
		{
			Internal::SSurfaceType_Mock* pSurface = pNode->m_lstSurfaceTypeInfo[nLayer].pSurfaceType;
			if (!pSurface)
				continue;

			for (int g = 0; g < pSurface->lstnVegetationGroups.Count(); g++)
			{
				int nGroupId = pSurface->lstnVegetationGroups[g];
				if (nGroupId < 0 || nGroupId >= (int)idxToLayer.size())
					continue;

				auto layerId = idxToLayer[nGroupId];
				if (layerId == MapLayers::EVegetationLayers::Unknown)
					continue;

				IStatInstGroup group;
				if (!gEnv->p3DEngine->GetStatInstGroup(nGroupId, group))
					continue;

				if (group.fSize <= 0.f)
					continue;

				float fSectorViewDistMax = fProcVegMaxViewDist * (fSectorSize / 64.f);
				float fSectorViewDistMin = fSectorViewDistMax * 0.5f;
				float fVegMaxViewDist = group.fVegRadius * group.fSize * group.fMaxViewDistRatio * fViewDistRatioVeg;

				if (fVegMaxViewDist > fSectorViewDistMax && pNode->m_nTreeLevel < (nProcVegMaxCacheLevels - 1))
					continue;
				if (fVegMaxViewDist < fSectorViewDistMin && pNode->m_nTreeLevel > 0)
					continue;
				if (nProcVeg >= 3 && pNode->m_nTreeLevel != nProcVeg - 3)
					continue;

				float fDensity = group.fDensity < 0.5f ? 0.5f : group.fDensity;
				float fGridSize = floor((fMaxX - fMinX) / fDensity);
				if (fGridSize <= 0.f)
					continue;

				fDensity = (fMaxX - fMinX) / fGridSize;
				float fOffset = fDensity * 0.5f;

				for (float fX = fMinX + fOffset; fX < fMaxX; fX += fDensity)
				{
					for (float fY = fMinY + fOffset; fY < fMaxY; fY += fDensity)
					{
						Vec3 vPos(fX + (rndGen.GenerateFloat() - 0.5f) * fDensity, fY + (rndGen.GenerateFloat() - 0.5f) * fDensity, 0);
						vPos.x = clamp_tpl(vPos.x, fMinX, fMaxX);
						vPos.y = clamp_tpl(vPos.y, fMinY, fMaxY);

						float fSurfaceTypeAmount = GetSurfaceTypeAmountCustom(pNode, vPos, pSurface->ucThisSurfaceTypeId);
						if (fSurfaceTypeAmount <= 0.5f)
							continue;

						vPos.z = gEnv->p3DEngine->GetTerrainZ(vPos.x, vPos.y);

						if (vPos.x < 0.f || vPos.x >= fTerrainSize || vPos.y < 0.f || vPos.y >= fTerrainSize)
							continue;

						float fScale = group.fSize + (rndGen.GenerateFloat() - 0.5f) * group.fSizeVar;
						if (fScale <= 0.f)
							continue;

						if (vPos.z < group.fElevationMin || vPos.z > group.fElevationMax)
							continue;

						if (group.fSlopeMin != 0.f || group.fSlopeMax != 255.f)
						{
							float sx = 0.0f, sy = 0.0f;

							if ((fX + fTerrainUnitSize) < fTerrainSize && fX >= fTerrainUnitSize)
								sx = gEnv->p3DEngine->GetTerrainZ(fX + fTerrainUnitSize, fY) - gEnv->p3DEngine->GetTerrainZ(fX - fTerrainUnitSize, fY);
							if ((fY + fTerrainUnitSize) < fTerrainSize && fY >= fTerrainUnitSize)
								sy = gEnv->p3DEngine->GetTerrainZ(fX, fY + fTerrainUnitSize) - gEnv->p3DEngine->GetTerrainZ(fX, fY - fTerrainUnitSize);

							Vec3 vNormal = Vec3(-sx, -sy, fTerrainUnitSize * 2.0f);
							vNormal.NormalizeFast();

							float fSlope = (1.0f - vNormal.z) * 255.0f;
							if (fSlope < group.fSlopeMin || fSlope > group.fSlopeMax)
								continue;
						}

						if (group.fVegRadius * fScale < fVegMinSize)
							continue;

						const uint32 angleRnd = rndGen.GenerateUint32();

						result.emplace_back(vPos, layerId);
					}
				}
			}
		}
	}

	float CEditorVegetationSource::GetSurfaceTypeAmountCustom(const Internal::CTerrainNode_Mock* pNode, const Vec3& vPos, uint8 ucGlobalSurfType)
	{
		if (!pNode->m_rangeInfo.pHMData || !pNode->m_rangeInfo.pSTPalette)
			return 0.0f;

		float fUnitSize = gEnv->p3DEngine->GetHeightMapUnitSize();

		float fUX = vPos.x / fUnitSize;
		float fUY = vPos.y / fUnitSize;

		int x1 = (int)fUX;
		int y1 = (int)fUY;
		int x2 = x1 + 1;
		int y2 = y1 + 1;

		float fDX = fUX - x1;
		float fDY = fUY - y1;

		auto getSurfGlobal = [pNode, fUnitSize](int globalX_units, int globalY_units) -> int
		{
			int localX = globalX_units - (int)(pNode->m_nOriginX / fUnitSize);
			int localY = globalY_units - (int)(pNode->m_nOriginY / fUnitSize);

			if (localX < 0)
				localX = 0;
			if (localY < 0)
				localY = 0;

			if (localX >= pNode->m_rangeInfo.nSize)
				localX = pNode->m_rangeInfo.nSize - 1;
			if (localY >= pNode->m_rangeInfo.nSize)
				localY = pNode->m_rangeInfo.nSize - 1;

			uint32 localType = pNode->m_rangeInfo.GetSurfaceType(localX, localY);
			if (localType >= Internal::SRangeInfo_Mock::e_index_undefined || !pNode->m_rangeInfo.pSTPalette)
				return -1;

			return pNode->m_rangeInfo.pSTPalette[localType];
		};

		float fS00 = (getSurfGlobal(x1, y1) == ucGlobalSurfType) ? 1.0f : 0.0f;
		float fS01 = (getSurfGlobal(x1, y2) == ucGlobalSurfType) ? 1.0f : 0.0f;
		float fS10 = (getSurfGlobal(x2, y1) == ucGlobalSurfType) ? 1.0f : 0.0f;
		float fS11 = (getSurfGlobal(x2, y2) == ucGlobalSurfType) ? 1.0f : 0.0f;

		if (fS00 > 0.f || fS01 > 0.f || fS10 > 0.f || fS11 > 0.f)
		{
			float fS0 = fS00 * (1.f - fDY) + fS01 * fDY;
			float fS1 = fS10 * (1.f - fDY) + fS11 * fDY;
			return fS0 * (1.f - fDX) + fS1 * fDX;
		}

		return 0.0f;
	}
}