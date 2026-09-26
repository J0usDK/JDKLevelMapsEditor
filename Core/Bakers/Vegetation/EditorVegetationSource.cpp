#include "StdAfx.h"
#include "EditorVegetationSource.h"

#include <emmintrin.h>
#include <IEditorImpl.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Vegetation/VegetationMap.h>
#include <Vegetation/VegetationObject.h>

#include <Includes/JDKMath/CpuFeatures.h>

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
	constexpr float kFltMax = 3.402823466e+38F;
	constexpr float kThreshold = 0.00001f;

	struct CEditorVegetationSource::SProcVegContext
	{
		const Internal::CTerrainNode_Mock* const* pGrid = nullptr;
		int nGridSize = 0;
		int nTerrainUnits = 0;
		int nUnitsToSectorBitShift = 0;

		float fTerrainSize = 0.0f;
		float fTerrainUnitSize = 0.0f;
		float fInvUnitSize = 0.0f;

		float fProcVegMaxViewDist = 0.0f;
		float fViewDistRatioVeg = 100.0f;
		float fVegMinSize = 0.0f;

		int nProcVegMaxCacheLevels = 1;
		int nProcVeg = 1;

		bool bNoRandomSeed = false;
	};
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

		SProcVegContext ctx;
		ctx.pGrid = level0.m_pData;
		ctx.nGridSize = level0.m_nSize;
		ctx.fTerrainSize = (float)gEnv->p3DEngine->GetTerrainSize();
		ctx.fTerrainUnitSize = gEnv->p3DEngine->GetHeightMapUnitSize();
		ctx.fInvUnitSize = 1.0f / ctx.fTerrainUnitSize;
		ctx.nTerrainUnits = (int)(ctx.fTerrainSize * ctx.fInvUnitSize);

		int nSectorSize = gEnv->p3DEngine->GetTerrainSectorSize();
		int nUnitsPerSector = (int)(nSectorSize * ctx.fInvUnitSize);
		while ((1 << ctx.nUnitsToSectorBitShift) < nUnitsPerSector)
			ctx.nUnitsToSectorBitShift++;

		if (ICVar* pCvar = gEnv->pConsole->GetCVar("e_ProcVegetationMaxViewDistance"))
			ctx.fProcVegMaxViewDist = pCvar->GetFVal();
		if (ICVar* pCvar = gEnv->pConsole->GetCVar("e_ViewDistRatioVegetation"))
			ctx.fViewDistRatioVeg = pCvar->GetFVal();
		if (ICVar* pCvar = gEnv->pConsole->GetCVar("e_VegetationMinSize"))
			ctx.fVegMinSize = pCvar->GetFVal();
		if (ICVar* pCvar = gEnv->pConsole->GetCVar("e_ProcVegetationMaxCacheLevels"))
			ctx.nProcVegMaxCacheLevels = pCvar->GetIVal();
		if (ICVar* pCvar = gEnv->pConsole->GetCVar("e_ProcVegetation"))
			ctx.nProcVeg = pCvar->GetIVal();
		if (ICVar* pCvar = gEnv->pConsole->GetCVar("bNoRandomSeed"))
			ctx.bNoRandomSeed = pCvar->GetIVal() != 0;

		for (int level = 0; level < pTerrain->m_arrSecInfoPyramid.m_nCount; level++)
		{
			const auto& levelGrid = pTerrain->m_arrSecInfoPyramid.m_pElements[level];
			if (!levelGrid.m_pData || levelGrid.m_nSize <= 0)
				continue;

			const int nGridSize = levelGrid.m_nSize;

			for (int i = 0; i < nGridSize * nGridSize; ++i)
			{
				const auto* pNode = levelGrid.m_pData[i];
				if (pNode)
				{
					__try
					{
						ProcessTerrainNode(pNode, ctx, idxToLayer, result);
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
	}

	void CEditorVegetationSource::ProcessTerrainNode(const Internal::CTerrainNode_Mock* pNode, const SProcVegContext& ctx, const std::vector<MapLayers::EVegetationLayers>& idxToLayer, std::vector<SVegetationInstanceData>& result)
	{
		CMTRand_int32 rndGen(ctx.bNoRandomSeed ? 0 : pNode->m_nOriginX + pNode->m_nOriginY);

		float fSectorSize = (float)(gEnv->p3DEngine->GetTerrainSectorSize() << pNode->m_nTreeLevel);

		float fMinX = (float)pNode->m_nOriginX;
		float fMinY = (float)pNode->m_nOriginY;
		float fMaxX = fMinX + fSectorSize;
		float fMaxY = fMinY + fSectorSize;

		const float fU2_4 = (ctx.fTerrainUnitSize * ctx.fTerrainUnitSize) * 4.0f;

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

				float fSectorViewDistMax = ctx.fProcVegMaxViewDist * (fSectorSize / 64.f);
				float fSectorViewDistMin = fSectorViewDistMax * 0.5f;
				float fVegMaxViewDist = group.fVegRadius * group.fSize * group.fMaxViewDistRatio * ctx.fViewDistRatioVeg;

				if (fVegMaxViewDist > fSectorViewDistMax && pNode->m_nTreeLevel < (ctx.nProcVegMaxCacheLevels - 1))
					continue;
				if (fVegMaxViewDist < fSectorViewDistMin && pNode->m_nTreeLevel > 0)
					continue;
				if (ctx.nProcVeg >= 3 && pNode->m_nTreeLevel != ctx.nProcVeg - 3)
					continue;

				float fDensity = group.fDensity < 0.5f ? 0.5f : group.fDensity;
				float fGridSize = floor((fMaxX - fMinX) / fDensity);
				if (fGridSize <= 0.f)
					continue;

				fDensity = (fMaxX - fMinX) / fGridSize;
				float fOffset = fDensity * 0.5f;

				float fSlopeMinSq = 0.0f;
				float fSlopeMaxSq = kFltMax;

				if (group.fSlopeMin > 0.0f)
				{
					float s = group.fSlopeMin / 255.0f;
					float k = (1.0f - s) * (1.0f - s);
					fSlopeMinSq = fU2_4 * (1.0f - k) / k;
				}
				if (group.fSlopeMax < 255.0f)
				{
					float s = group.fSlopeMax / 255.0f;
					float k = (1.0f - s) * (1.0f - s);
					if (k > kThreshold)
						fSlopeMaxSq = fU2_4 * (1.0f - k) / k;
				}

				for (float fX = fMinX + fOffset; fX < fMaxX; fX += fDensity)
				{
					for (float fY = fMinY + fOffset; fY < fMaxY; fY += fDensity)
					{
						Vec3 vPos(fX + (rndGen.GenerateFloat() - 0.5f) * fDensity, fY + (rndGen.GenerateFloat() - 0.5f) * fDensity, 0);
						vPos.x = clamp_tpl(vPos.x, fMinX, fMaxX);
						vPos.y = clamp_tpl(vPos.y, fMinY, fMaxY);

						float fSurfaceTypeAmount = GetSurfaceTypeAmountCustom(ctx, vPos, pSurface->ucThisSurfaceTypeId);
						if (fSurfaceTypeAmount <= 0.5f)
							continue;

						vPos.z = GetTerrainZFast(ctx, vPos.x, vPos.y);

						if (vPos.x < 0.f || vPos.x >= ctx.fTerrainSize || vPos.y < 0.f || vPos.y >= ctx.fTerrainSize)
							continue;

						float fScale = group.fSize + (rndGen.GenerateFloat() - 0.5f) * group.fSizeVar;
						if (fScale <= 0.f)
							continue;

						if (vPos.z < group.fElevationMin || vPos.z > group.fElevationMax)
							continue;

						if (group.fSlopeMin != 0.f || group.fSlopeMax != 255.f)
						{
							float sx = 0.0f, sy = 0.0f;

							if ((fX + ctx.fTerrainUnitSize) < ctx.fTerrainSize && fX >= ctx.fTerrainUnitSize)
								sx = GetTerrainZFast(ctx, fX + ctx.fTerrainUnitSize, fY) - GetTerrainZFast(ctx, fX - ctx.fTerrainUnitSize, fY);
							if ((fY + ctx.fTerrainUnitSize) < ctx.fTerrainSize && fY >= ctx.fTerrainUnitSize)
								sy = GetTerrainZFast(ctx, fX, fY + ctx.fTerrainUnitSize) - GetTerrainZFast(ctx, fX, fY - ctx.fTerrainUnitSize);

							float fGradSq = (sx * sx) + (sy * sy);
							if (fGradSq < fSlopeMinSq || fGradSq > fSlopeMaxSq)
								continue;
						}

						if (group.fVegRadius * fScale < ctx.fVegMinSize)
							continue;

						const uint32 angleRnd = rndGen.GenerateUint32();

						result.emplace_back(vPos, layerId);
					}
				}
			}
		}
	}

	float CEditorVegetationSource::GetTerrainZFast(const SProcVegContext& ctx, float x, float y)
	{
		int xu = (int)(x * ctx.fInvUnitSize);
		int yu = (int)(y * ctx.fInvUnitSize);

		if (xu < 0) xu = 0;
		else if (xu >= ctx.nTerrainUnits) xu = ctx.nTerrainUnits - 1;

		if (yu < 0) yu = 0;
		else if (yu >= ctx.nTerrainUnits) yu = ctx.nTerrainUnits - 1;

		int secX = xu >> ctx.nUnitsToSectorBitShift;
		int secY = yu >> ctx.nUnitsToSectorBitShift;

		if (secX < 0 || secX >= ctx.nGridSize || secY < 0 || secY >= ctx.nGridSize)
			return 0.0f;

		const auto* pNode = ctx.pGrid[secX * ctx.nGridSize + secY];
		if (!pNode || !pNode->m_rangeInfo.pHMData)
			return 0.0f;

		const auto& ri = pNode->m_rangeInfo;
		int nMask = ri.nSize - 2;

		if (ri.nUnitBitShift == 0)
		{
			int localX = xu & nMask;
			int localY = yu & nMask;
			uint32 idx = localX * ri.nSize + localY;
			return ri.fOffset + (float)(ri.pHMData[idx].height) * ri.fRange;
		}
		else
		{
			float fInvStep = (ri.nSize > 1) ? (1.f / ((1 << ctx.nUnitsToSectorBitShift) / (ri.nSize - 1))) : 1.f;

			int nX = xu >> ri.nUnitBitShift;
			int nY = yu >> ri.nUnitBitShift;

			float fX = (xu * fInvStep) - nX;
			float fY = (yu * fInvStep) - nY;

			nX &= nMask;
			nY &= nMask;

			auto getHeight = [&ri](int lx, int ly) -> float
			{
				uint32 idx = lx * ri.nSize + ly;
				return ri.fOffset + static_cast<float>(ri.pHMData[idx].height) * ri.fRange;
			};

			return getHeight(nX, nY) * (1.f - fX) * (1.f - fY) +
				getHeight(nX + 1, nY) * fX * (1.f - fY) +
				getHeight(nX, nY + 1) * (1.f - fX) * fY +
				getHeight(nX + 1, nY + 1) * fX * fY;
		}
	}

	float CEditorVegetationSource::GetSurfaceTypeAmountCustom(const SProcVegContext& ctx, const Vec3& vPos, uint8 ucGlobalSurfType)
	{
		int xu = static_cast<int>(vPos.x * ctx.fInvUnitSize);
		int yu = static_cast<int>(vPos.y * ctx.fInvUnitSize);
		
		if (xu < 0) xu = 0;
		else if (xu >= ctx.nTerrainUnits) xu = ctx.nTerrainUnits - 1;

		if (yu < 0) yu = 0;
		else if (yu >= ctx.nTerrainUnits) yu = ctx.nTerrainUnits - 1;

		int secX = xu >> ctx.nUnitsToSectorBitShift;
		int secY = yu >> ctx.nUnitsToSectorBitShift;
		if (secX < 0 || secX >= ctx.nGridSize || secY < 0 || secY >= ctx.nGridSize)
			return 0.0f;

		const auto* pLevel0Node = ctx.pGrid[secX * ctx.nGridSize + secY];
		if (!pLevel0Node)
			return 0.0f;

		const auto& ri = pLevel0Node->m_rangeInfo;
		if (!ri.pHMData || !ri.pSTPalette)
			return 0.0f;

		float fLocalUX = (vPos.x - static_cast<float>(pLevel0Node->m_nOriginX)) * ctx.fInvUnitSize;
		float fLocalUY = (vPos.y - static_cast<float>(pLevel0Node->m_nOriginY)) * ctx.fInvUnitSize;

		int lx1 = static_cast<int>(fLocalUX);
		int ly1 = static_cast<int>(fLocalUY);

		if (lx1 < 0) lx1 = 0;
		if (ly1 < 0) ly1 = 0;

		int nMaxIdx = static_cast<int>(ri.nSize) - 1;
		int lx2 = lx1 + 1;
		int ly2 = ly1 + 1;

		if (lx1 > nMaxIdx) lx1 = nMaxIdx;
		if (ly1 > nMaxIdx) ly1 = nMaxIdx;
		if (lx2 > nMaxIdx) lx2 = nMaxIdx;
		if (ly2 > nMaxIdx) ly2 = nMaxIdx;

		float fDX = fLocalUX - static_cast<float>(lx1);
		float fDY = fLocalUY - static_cast<float>(ly1);

		auto getGlobalType = [&ri](int lx, int ly) -> int
		{
			uint32 localType = ri.GetSurfaceType(lx, ly);
			return (localType < Internal::SRangeInfo_Mock::e_index_undefined) ? ri.pSTPalette[localType] : -1;
		};

		int t00 = getGlobalType(lx1, ly1);
		int t01 = getGlobalType(lx1, ly2);
		int t10 = getGlobalType(lx2, ly1);
		int t11 = getGlobalType(lx2, ly2);

		if (JDK::Math::GetCpuFeatures().bHasSSE2)
		{
			__m128i vTypes = _mm_setr_epi32(t00, t01, t10, t11);
			__m128i vTarget = _mm_set1_epi32(ucGlobalSurfType);

			__m128i vCmp = _mm_cmpeq_epi32(vTypes, vTarget);
			if (_mm_movemask_epi8(vCmp) == 0)
				return 0.0f;

			__m128 vOnes = _mm_set1_ps(1.0f);
			__m128 vWeights = _mm_and_ps(_mm_castsi128_ps(vCmp), vOnes);

			__m128 vS_00_10 = _mm_shuffle_ps(vWeights, vWeights, _MM_SHUFFLE(2, 0, 2, 0));
			__m128 vS_01_11 = _mm_shuffle_ps(vWeights, vWeights, _MM_SHUFFLE(3, 1, 3, 1));

			__m128 vDy = _mm_set1_ps(fDY);
			__m128 vOneMinusDy = _mm_sub_ps(vOnes, vDy);

			__m128 vS0_S1 = _mm_add_ps(_mm_mul_ps(vS_00_10, vOneMinusDy), _mm_mul_ps(vS_01_11, vDy));

			float fS0 = _mm_cvtss_f32(vS0_S1);
			float fS1 = _mm_cvtss_f32(_mm_shuffle_ps(vS0_S1, vS0_S1, _MM_SHUFFLE(1, 1, 1, 1)));

			return fS0 * (1.0f - fDX) + fS1 * fDX;
		}
		else
		{
			float fS00 = (t00 == ucGlobalSurfType) ? 1.0f : 0.0f;
			float fS01 = (t01 == ucGlobalSurfType) ? 1.0f : 0.0f;
			float fS10 = (t10 == ucGlobalSurfType) ? 1.0f : 0.0f;
			float fS11 = (t11 == ucGlobalSurfType) ? 1.0f : 0.0f;

			if (fS00 > 0.f || fS01 > 0.f || fS10 > 0.f || fS11 > 0.f)
			{
				float fS0 = fS00 * (1.f - fDY) + fS01 * fDY;
				float fS1 = fS10 * (1.f - fDY) + fS11 * fDY;
				return fS0 * (1.f - fDX) + fS1 * fDX;
			}
		}

		return 0.0f;
	}
}