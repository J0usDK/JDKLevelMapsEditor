#include "StdAfx.h"
#include "EditorVegetationSource.h"

#include <algorithm>
#include <optional>
#include <limits>
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

namespace JDKLevelMaps::JDKEditorSource
{
	namespace
	{
		constexpr float kFltMax = 3.402823466e+38F;
		constexpr float kTerrainBottomLevel = 0;
		constexpr float kThreshold = 0.00001f;

		constexpr bool kJitterYDrawnBeforeX = true;

		bool s_bProcVegUsed = false;
		bool s_bLayoutValid = false;

		struct SProcVegNodeBounds
		{
			float fMinX = 0.0f;
			float fMinY = 0.0f;
			float fMaxX = 0.0f;
			float fMaxY = 0.0f;

			[[nodiscard]] static SProcVegNodeBounds Create(const Internal::CTerrainNode_Mock& node)
			{
				SProcVegNodeBounds bounds;
				float fSectorSize = static_cast<float>(gEnv->p3DEngine->GetTerrainSectorSize() << node.m_nTreeLevel);

				bounds.fMinX = static_cast<float>(node.m_nOriginX);
				bounds.fMinY = static_cast<float>(node.m_nOriginY);
				bounds.fMaxX = bounds.fMinX + fSectorSize;
				bounds.fMaxY = bounds.fMinY + fSectorSize;

				return bounds;
			}
		};

		struct SSlopeRange
		{
			float fMinSq = 0.0f;
			float fMaxSq = kFltMax;

			bool bActive = false;

			[[nodiscard]] bool Contains(float value) const noexcept
			{
				return value >= fMinSq && value <= fMaxSq;
			}

			[[nodiscard]] static SSlopeRange Create(const IStatInstGroup& group, float fU2_4) noexcept
			{
				SSlopeRange range;
				range.bActive = (group.fSlopeMin != 0.0f || group.fSlopeMax != 255.0f);

				if (group.fSlopeMin > 0.0f)
				{
					const float s = group.fSlopeMin / 255.0f;
					const float k = (1.0f - s) * (1.0f - s);

					range.fMinSq = fU2_4 * (1.0f - k) / k;
				}

				if (group.fSlopeMax < 255.0f)
				{
					const float s = group.fSlopeMax / 255.0f;
					const float k = (1.0f - s) * (1.0f - s);

					if (k > kThreshold)
						range.fMaxSq = fU2_4 * (1.0f - k) / k;
				}

				return range;
			}
		};

		struct SProcVegContext
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
			int nMaxObjectsPerSector = std::numeric_limits<int>::max();

			bool bNoRandomSeed = false;

			[[nodiscard]] static SProcVegContext Create(const Internal::CTerrainNode_Mock* const* pGridData, int nSize)
			{
				SProcVegContext ctx;
				ctx.pGrid = pGridData;
				ctx.nGridSize = nSize;
				ctx.fTerrainSize = static_cast<float>(gEnv->p3DEngine->GetTerrainSize());
				ctx.fTerrainUnitSize = gEnv->p3DEngine->GetHeightMapUnitSize();
				ctx.fInvUnitSize = 1.0f / ctx.fTerrainUnitSize;
				ctx.nTerrainUnits = static_cast<int>(ctx.fTerrainSize * ctx.fInvUnitSize);

				int nSectorSize = gEnv->p3DEngine->GetTerrainSectorSize();
				int nUnitsPerSector = static_cast<int>(nSectorSize * ctx.fInvUnitSize);

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
				if (ICVar* pCvar = gEnv->pConsole->GetCVar("e_ProcVegetationMaxObjectsPerSector"))
					ctx.nMaxObjectsPerSector = pCvar->GetIVal();

				if (ctx.nProcVeg < 1)
					ctx.nProcVeg = 1;

				ctx.bNoRandomSeed = gEnv->bNoRandomSeed;

				return ctx;
			}
		};

		struct SProcVegGroupParams
		{
			MapLayers::EVegetationLayers layer = MapLayers::EVegetationLayers::Unknown;
			IStatInstGroup group;

			float fDensity = 0.0f;
			float fOffset = 0.0f;

			SSlopeRange slope;

			[[nodiscard]] static std::optional<SProcVegGroupParams> TryCreate(const SProcVegContext& ctx, const Internal::CTerrainNode_Mock& node, MapLayers::EVegetationLayers layer, int nGroupId)
			{
				IStatInstGroup group;
				if (!gEnv->p3DEngine->GetStatInstGroup(nGroupId, group))
					return std::nullopt;
				
				static bool s_bWarnedIdMismatch = false;
				if (group.nID != nGroupId && !s_bWarnedIdMismatch)
				{
					s_bWarnedIdMismatch = true;
					JDK_WARN("Vegetation group index %d has nID %d. Procedural vegetation may not match the engine for such groups.", nGroupId, group.nID);
				}

				if (!group.pStatObj)
					return std::nullopt;
				if (group.fSize <= 0.f)
					return std::nullopt;
				if (ctx.nProcVeg >= 3 && node.m_nTreeLevel != ctx.nProcVeg - 3)
					return std::nullopt;

				SProcVegGroupParams params;
				params.group = group;
				params.layer = layer;

				const float fSectorSize = static_cast<float>(gEnv->p3DEngine->GetTerrainSectorSize() << node.m_nTreeLevel);

				const float fSectorViewDistMax = ctx.fProcVegMaxViewDist * (fSectorSize / 64.0f);
				const float fSectorViewDistMin = fSectorViewDistMax * 0.5f;
				const float fVegMaxViewDist = group.fVegRadius * group.fSize * group.fMaxViewDistRatio * ctx.fViewDistRatioVeg;

				if (fVegMaxViewDist > fSectorViewDistMax && node.m_nTreeLevel < (ctx.nProcVegMaxCacheLevels - 1))
					return std::nullopt;
				if (fVegMaxViewDist < fSectorViewDistMin && node.m_nTreeLevel > 0)
					return std::nullopt;

				const float fMinX = static_cast<float>(node.m_nOriginX);
				const float fMaxX = fMinX + fSectorSize;

				params.fDensity = group.fDensity < 0.5f ? 0.5f : group.fDensity;

				const float fU2_4 = (ctx.fTerrainUnitSize * ctx.fTerrainUnitSize) * 4.0f;
				const float fGridSize = floor((fMaxX - fMinX) / params.fDensity);

				if (fGridSize <= 0.0f)
					return std::nullopt;

				params.fDensity = (fMaxX - fMinX) / fGridSize;
				params.fOffset = params.fDensity * 0.5f;
				params.slope = SSlopeRange::Create(group, fU2_4);

				return params;
			}
		};
	}

	namespace
	{
		[[nodiscard]] Vec2i WorldToTerrainUnits(const SProcVegContext& ctx, float x, float y)
		{
			Vec2i units;

			units.x = static_cast<int>(x * ctx.fInvUnitSize);
			units.y = static_cast<int>(y * ctx.fInvUnitSize);

			if (units.x < 0) units.x = 0;
			else if (units.x >= ctx.nTerrainUnits) units.x = ctx.nTerrainUnits - 1;

			if (units.y < 0) units.y = 0;
			else if (units.y >= ctx.nTerrainUnits) units.y = ctx.nTerrainUnits - 1;

			return units;
		}

		[[nodiscard]] const Internal::CTerrainNode_Mock* GetNodeAtUnits(const SProcVegContext& ctx, Vec2i units)
		{
			int secX = units.x >> ctx.nUnitsToSectorBitShift;
			int secY = units.y >> ctx.nUnitsToSectorBitShift;

			if (secX < 0 || secX >= ctx.nGridSize || secY < 0 || secY >= ctx.nGridSize)
				return nullptr;

			return ctx.pGrid[secX * ctx.nGridSize + secY];
		}

		[[nodiscard]] float HeightLocal(const Internal::SRangeInfo_Mock& ri, int lx, int ly) noexcept
		{
			const uint32 idx = static_cast<uint32>(lx) * ri.nSize + static_cast<uint32>(ly);
			return ri.fOffset + static_cast<float>(ri.pHMData[idx].height) * ri.fRange;
		}

		[[nodiscard]] float GetTerrainZUnits(const SProcVegContext& ctx, int unitX, int unitY)
		{
			unitX = std::clamp(unitX, 0, ctx.nTerrainUnits - 1);
			unitY = std::clamp(unitY, 0, ctx.nTerrainUnits - 1);

			const auto* pNode = GetNodeAtUnits(ctx, Vec2i(unitX, unitY));
			if (!pNode || !pNode->m_rangeInfo.pHMData)
				return 0.0f;

			const auto& ri = pNode->m_rangeInfo;
			int nMask = ri.nSize - 2;

			if (ri.nUnitBitShift == 0)
				return HeightLocal(ri, unitX & nMask, unitY & nMask);
			

			float fInvStep = (ri.nSize > 1) ? (1.f / ((1 << ctx.nUnitsToSectorBitShift) / (ri.nSize - 1))) : 1.f;

			int nX = unitX >> ri.nUnitBitShift;
			int nY = unitY >> ri.nUnitBitShift;

			float fX = (unitX * fInvStep) - nX;
			float fY = (unitY * fInvStep) - nY;

			nX &= nMask;
			nY &= nMask;

			return HeightLocal(ri, nX, nY) * (1.f - fX) * (1.f - fY) +
				HeightLocal(ri, nX + 1, nY) * fX * (1.f - fY) +
				HeightLocal(ri, nX, nY + 1) * (1.f - fX) * fY +
				HeightLocal(ri, nX + 1, nY + 1) * fX * fY;
		}

		[[nodiscard]] float GetTerrainZFast(const SProcVegContext& ctx, float x, float y)
		{
			const Vec2i units = WorldToTerrainUnits(ctx, x, y);
			return GetTerrainZUnits(ctx, units.x, units.y);
		}

		[[nodiscard]] float GetTerrainZApr(const SProcVegContext& ctx, float x, float y)
		{
			const float uX = x * ctx.fInvUnitSize;
			const float uY = y * ctx.fInvUnitSize;

			const int nX = static_cast<int>(uX);
			const int nY = static_cast<int>(uY);

			if (!(x > 0.0f && y > 0.0f) || nX < 0 || nY < 0 || nX >= ctx.nTerrainUnits || nY >= ctx.nTerrainUnits)
				return kTerrainBottomLevel;

			const float fX = uX - nX;
			const float fY = uY - nY;

			const float z00 = GetTerrainZUnits(ctx, nX, nY);
			const float z10 = GetTerrainZUnits(ctx, nX + 1, nY);
			const float z01 = GetTerrainZUnits(ctx, nX, nY + 1);

			if (fX + fY < 1.f)
				return z00 * (1.f - fX - fY) + z10 * fX + z01 * fY;

			const float z11 = GetTerrainZUnits(ctx, nX + 1, nY + 1);
			return z11 * (fX + fY - 1.f) + z01 * (1.f - fX) + z10 * (1.f - fY);
		}

		[[nodiscard]] std::vector<MapLayers::EVegetationLayers> BuildGroupLookup(CVegetationMap* pVegetationMap, const Settings::SVegetationBakerSettings& settings)
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
				if (idx < 0 || idx >= static_cast<int>(result.size()))
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

		[[nodiscard]] float GetSurfaceTypeAmountScalar(int t00, int t01, int t10, int t11, uint8 ucGlobalSurfType, float fDX, float fDY) noexcept
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
			return 0.0f;
		}

		[[nodiscard]] float GetSurfaceTypeAmountSSE2(int t00, int t01, int t10, int t11, uint8 ucGlobalSurfType, float fDX, float fDY) noexcept
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

		[[nodiscard]] float GetSurfaceTypeAmountCustom(const SProcVegContext& ctx, const Vec3& vPos, uint8 ucGlobalSurfType)
		{
			Vec2i units = WorldToTerrainUnits(ctx, vPos.x, vPos.y);

			const auto* pNode = GetNodeAtUnits(ctx, units);
			if (!pNode)
				return 0.0f;

			const auto& ri = pNode->m_rangeInfo;
			if (!ri.pHMData || !ri.pSTPalette)
				return 0.0f;

			float fLocalUX = (vPos.x - static_cast<float>(pNode->m_nOriginX)) * ctx.fInvUnitSize;
			float fLocalUY = (vPos.y - static_cast<float>(pNode->m_nOriginY)) * ctx.fInvUnitSize;

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
				return GetSurfaceTypeAmountSSE2(t00, t01, t10, t11, ucGlobalSurfType, fDX, fDY);
			else
				return GetSurfaceTypeAmountScalar(t00, t01, t10, t11, ucGlobalSurfType, fDX, fDY);
		}

		[[nodiscard]] bool GenerateGroupInstances(const SProcVegContext& ctx, const Internal::SSurfaceType_Mock& surface, const SProcVegGroupParams& params, const SProcVegNodeBounds& bounds, CMTRand_int32& rndGen, bool bEmit, int& nInstancesCounter, std::vector<SVegetationInstanceData>& result)
		{
			for (float fX = bounds.fMinX + params.fOffset; fX < bounds.fMaxX; fX += params.fDensity)
			{
				for (float fY = bounds.fMinY + params.fOffset; fY < bounds.fMaxY; fY += params.fDensity)
				{
					const float fDrawFirst = rndGen.GenerateFloat();
					const float fDrawSecond = rndGen.GenerateFloat();
					const float fJitterX = kJitterYDrawnBeforeX ? fDrawSecond : fDrawFirst;
					const float fJitterY = kJitterYDrawnBeforeX ? fDrawFirst : fDrawSecond;

					Vec3 vPos(fX + (fJitterX - 0.5f) * params.fDensity, fY + (fJitterY - 0.5f) * params.fDensity, 0.0f);
					vPos.x = clamp_tpl(vPos.x, bounds.fMinX, bounds.fMaxX);
					vPos.y = clamp_tpl(vPos.y, bounds.fMinY, bounds.fMaxY);

					float fSurfaceTypeAmount = GetSurfaceTypeAmountCustom(ctx, vPos, surface.ucThisSurfaceTypeId);
					if (fSurfaceTypeAmount <= 0.5f)
						continue;

					vPos.z = GetTerrainZApr(ctx, vPos.x, vPos.y);

					if (vPos.x < 0.f || vPos.x >= ctx.fTerrainSize || vPos.y < 0.f || vPos.y >= ctx.fTerrainSize)
						continue;

					float fScale = params.group.fSize + (rndGen.GenerateFloat() - 0.5f) * params.group.fSizeVar;
					if (fScale <= 0.f)
						continue;

					if (vPos.z < params.group.fElevationMin || vPos.z > params.group.fElevationMax)
						continue;

					if (params.slope.bActive)
					{
						float sx = 0.0f, sy = 0.0f;

						if ((fX + ctx.fTerrainUnitSize) < ctx.fTerrainSize && fX >= ctx.fTerrainUnitSize)
							sx = GetTerrainZFast(ctx, fX + ctx.fTerrainUnitSize, fY) - GetTerrainZFast(ctx, fX - ctx.fTerrainUnitSize, fY);
						if ((fY + ctx.fTerrainUnitSize) < ctx.fTerrainSize && fY >= ctx.fTerrainUnitSize)
							sy = GetTerrainZFast(ctx, fX, fY + ctx.fTerrainUnitSize) - GetTerrainZFast(ctx, fX, fY - ctx.fTerrainUnitSize);

						float fGradSq = (sx * sx) + (sy * sy);
						if (!params.slope.Contains(fGradSq))
							continue;
					}

					if (params.group.fVegRadius * fScale < ctx.fVegMinSize)
						continue;

					// Keep the RNG sequence in sync with the original engine implementation.
					const uint32 angleRnd = rndGen.GenerateUint32();

					if (bEmit)
						result.emplace_back(vPos, params.layer);

					if (++nInstancesCounter >= ctx.nMaxObjectsPerSector)
						return false;
				}
			}

			return true;
		}

		[[nodiscard]] bool IsBakedGroup(int nGroupId, const std::vector<MapLayers::EVegetationLayers>& idxToLayer) noexcept
		{
			return nGroupId >= 0 && nGroupId < static_cast<int>(idxToLayer.size()) && idxToLayer[nGroupId] != MapLayers::EVegetationLayers::Unknown;
		}

		void ProcessTerrainNode(const Internal::CTerrainNode_Mock& node, const SProcVegContext& ctx, const std::vector<MapLayers::EVegetationLayers>& idxToLayer, std::vector<SVegetationInstanceData>& result)
		{
			int nLastBakedPos = -1;
			{
				int nPos = 0;
				for (int nLayer = 0; nLayer < node.m_lstSurfaceTypeInfo.Count(); nLayer++)
				{
					const Internal::SSurfaceType_Mock* pSurface = node.m_lstSurfaceTypeInfo[nLayer].pSurfaceType;
					if (!pSurface)
						continue;

					for (int g = 0; g < pSurface->lstnVegetationGroups.Count(); g++, nPos++)
						if (IsBakedGroup(pSurface->lstnVegetationGroups[g], idxToLayer))
							nLastBakedPos = nPos;
				}
			}

			if (nLastBakedPos < 0)
				return;

			CMTRand_int32 rndGen(ctx.bNoRandomSeed ? 0 : node.m_nOriginX + node.m_nOriginY);
			SProcVegNodeBounds nodeBounds = SProcVegNodeBounds::Create(node);

			int nInstancesCounter = 0;
			int nPos = 0;

			for (int nLayer = 0; nLayer < node.m_lstSurfaceTypeInfo.Count(); nLayer++)
			{
				Internal::SSurfaceType_Mock* pSurface = node.m_lstSurfaceTypeInfo[nLayer].pSurfaceType;
				if (!pSurface)
					continue;

				for (int g = 0; g < pSurface->lstnVegetationGroups.Count(); g++, nPos++)
				{
					if (nPos > nLastBakedPos)
						return;

					const int nGroupId = pSurface->lstnVegetationGroups[g];
					if (nGroupId < 0)
						continue;

					const bool bEmit = IsBakedGroup(nGroupId, idxToLayer);
					const auto layer = bEmit ? idxToLayer[nGroupId] : MapLayers::EVegetationLayers::Unknown;

					auto params = SProcVegGroupParams::TryCreate(ctx, node, layer, nGroupId);
					if (!params)
						continue;

					if (!GenerateGroupInstances(ctx, *pSurface, *params, nodeBounds, rndGen, bEmit, nInstancesCounter, result))
						return; // limit reached
				}
			}
		}

		void CollectProcVegetation(const std::vector<MapLayers::EVegetationLayers>& idxToLayer, std::vector<SVegetationInstanceData>& result)
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

			SProcVegContext ctx = SProcVegContext::Create(level0.m_pData, level0.m_nSize);

			const int nLevelsToProcess = std::min(pTerrain->m_arrSecInfoPyramid.m_nCount, ctx.nProcVegMaxCacheLevels);

			for (int level = 0; level < nLevelsToProcess; level++)
			{
				const auto& levelGrid = pTerrain->m_arrSecInfoPyramid.m_pElements[level];
				if (!levelGrid.m_pData || levelGrid.m_nSize <= 0)
					continue;

				const int nGridSize = levelGrid.m_nSize;

				for (int i = 0; i < nGridSize * nGridSize; ++i)
				{
					const auto* pNode = levelGrid.m_pData[i];
					if (!pNode)
						continue;

					__try
					{
						ProcessTerrainNode(*pNode, ctx, idxToLayer, result);
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

	std::vector<SVegetationInstanceData> QueryVegetationInstances(float x1, float y1, float x2, float y2, const Settings::SVegetationBakerSettings& settings)
	{
		std::vector<SVegetationInstanceData> result;

		CEditorImpl* pEditorImpl = static_cast<CEditorImpl*>(GetIEditor());
		if (!pEditorImpl)
			return {};

		CVegetationMap* pVegetationMap = pEditorImpl->GetVegetationMap();
		if (!pVegetationMap)
			return {};

		const auto& idxToLayer = BuildGroupLookup(pVegetationMap, settings);

		std::vector<CVegetationInstance*> instances;
		pVegetationMap->GetObjectInstances(x1, y1, x2, y2, instances);

		if (!Utils::Common::TryReserve(result, instances.size()))
			return {};

		for (auto pInstance : instances)
		{
			if (!pInstance || !pInstance->object)
				continue;

			const int idx = pInstance->object->GetId();

			if (idx < 0 || idx >= static_cast<int>(idxToLayer.size()))
				continue;

			const auto layer = idxToLayer[idx];
			if (layer != MapLayers::EVegetationLayers::Unknown)
				result.emplace_back(pInstance->pos, layer);
		}

		if (s_bProcVegUsed)
		{
			if (!s_bLayoutValid)
				s_bLayoutValid = Internal::ValidateMockLayout();

			if (s_bLayoutValid)
				CollectProcVegetation(idxToLayer, result);
			else
				JDK_WARN("Found layout discrepancies. Procedural Vegetation has been disabled.\n"
					"Check the engine version and update the mocked structures and classes.");
		}

		return result;
	}
}