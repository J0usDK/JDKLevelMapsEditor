#include "StdAfx.h"
#include "ProcVegGenerator.h"

#include <algorithm>
#include <optional>
#include <limits>
#include <emmintrin.h>

#include <CryMath/MTPseudoRandom.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IStatObj.h>

#include <Includes/JDKMath/CpuFeatures.h>

#include "MockTerrain.h"
#include "VegetationData.h"
#include "Core/Data/Common.h"
#include "Utils/Logger.h"

namespace JDKLevelMaps::JDKEditorSource::Internal
{
	namespace
	{
		constexpr float kFltMax = 3.402823466e+38F;
		constexpr float kTerrainBottomLevel = 0;
		constexpr float kThreshold = 0.00001f;
		constexpr float kAngleByteToRadians = gf_PI2 / 255.0f;

		constexpr bool kJitterYDrawnBeforeX = true;

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
			const CTerrainNode_Mock* const* pGrid = nullptr;
			int nGridSize = 0;
			int nTerrainUnits = 0;
			int nUnitsToSectorBitShift = 0;

			float fTerrainSize = 0.0f;
			float fTerrainUnitSize = 0.0f;
			float fInvUnitSize = 0.0f;

			float fProcVegMaxViewDist = 0.0f;
			float fViewDistRatioVeg = 100.0f;
			float fVegMinSize = 0.0f;

			float fU2_4 = 0.0f;

			int nProcVegMaxCacheLevels = 1;
			int nProcVeg = 1;
			int nMaxObjectsPerSector = std::numeric_limits<int>::max();

			bool bHasSSE2 = false;
			bool bNoRandomSeed = false;

			[[nodiscard]] static SProcVegContext Create(const CTerrainNode_Mock* const* pGridData, int nSize)
			{
				SProcVegContext ctx;
				ctx.pGrid = pGridData;
				ctx.nGridSize = nSize;
				ctx.fTerrainSize = static_cast<float>(gEnv->p3DEngine->GetTerrainSize());
				ctx.fTerrainUnitSize = gEnv->p3DEngine->GetHeightMapUnitSize();
				ctx.fInvUnitSize = 1.0f / ctx.fTerrainUnitSize;
				ctx.nTerrainUnits = static_cast<int>(ctx.fTerrainSize * ctx.fInvUnitSize);
				ctx.fU2_4 = (ctx.fTerrainUnitSize * ctx.fTerrainUnitSize) * 4.0f;

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

				ctx.bHasSSE2 = JDK::Math::GetCpuFeatures().bHasSSE2;
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

			int nGroupId = -1;

			SSlopeRange slope;

			[[nodiscard]] static std::optional<SProcVegGroupParams> TryCreate(const SProcVegContext& ctx, const CTerrainNode_Mock& node, MapLayers::EVegetationLayers layer, int nGroupId)
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

				const float sectorSize = static_cast<float>(gEnv->p3DEngine->GetTerrainSectorSize() << node.m_nTreeLevel);

				const float sectorViewDistMax = ctx.fProcVegMaxViewDist * (sectorSize / 64.0f);
				const float sectorViewDistMin = sectorViewDistMax * 0.5f;
				const float vegMaxViewDist = group.fVegRadius * group.fSize * group.fMaxViewDistRatio * ctx.fViewDistRatioVeg;

				if (vegMaxViewDist > sectorViewDistMax && node.m_nTreeLevel < (ctx.nProcVegMaxCacheLevels - 1))
					return std::nullopt;
				if (vegMaxViewDist < sectorViewDistMin && node.m_nTreeLevel > 0)
					return std::nullopt;

				const float minX = static_cast<float>(node.m_nOriginX);
				const float maxX = minX + sectorSize;

				params.fDensity = group.fDensity < 0.5f ? 0.5f : group.fDensity;

				const float gridSize = floor((maxX - minX) / params.fDensity);
				if (gridSize <= 0.0f)
					return std::nullopt;

				params.fDensity = (maxX - minX) / gridSize;
				params.fOffset = params.fDensity * 0.5f;
				params.nGroupId = nGroupId;
				params.slope = SSlopeRange::Create(group, ctx.fU2_4);

				return params;
			}
		};
	}

	namespace
	{
		[[nodiscard]] Data::SBounds CreateProcVegBounds(const CTerrainNode_Mock& node)
		{
			Data::SBounds bounds;
			float fSectorSize = static_cast<float>(gEnv->p3DEngine->GetTerrainSectorSize() << node.m_nTreeLevel);

			bounds.fMinX = static_cast<float>(node.m_nOriginX);
			bounds.fMinY = static_cast<float>(node.m_nOriginY);
			bounds.fMaxX = bounds.fMinX + fSectorSize;
			bounds.fMaxY = bounds.fMinY + fSectorSize;

			return bounds;
		}

		[[nodiscard]] Vec2i WorldToTerrainUnits(const SProcVegContext& ctx, const Vec2& pos)
		{
			Vec2i units = pos * ctx.fInvUnitSize;

			units.x = std::clamp(units.x, 0, ctx.nTerrainUnits - 1);
			units.y = std::clamp(units.y, 0, ctx.nTerrainUnits - 1);

			return units;
		}

		[[nodiscard]] const CTerrainNode_Mock* GetNodeAtUnits(const SProcVegContext& ctx, Vec2i units)
		{
			int secX = units.x >> ctx.nUnitsToSectorBitShift;
			int secY = units.y >> ctx.nUnitsToSectorBitShift;

			if (secX < 0 || secX >= ctx.nGridSize || secY < 0 || secY >= ctx.nGridSize)
				return nullptr;

			return ctx.pGrid[secX * ctx.nGridSize + secY];
		}

		[[nodiscard]] float HeightLocal(const SRangeInfo_Mock& ri, int lx, int ly) noexcept
		{
			const uint32 idx = static_cast<uint32>(lx) * ri.nSize + static_cast<uint32>(ly);
			return ri.fOffset + static_cast<float>(ri.pHMData[idx].height) * ri.fRange;
		}

		[[nodiscard]] float GetTerrainZUnits(const SProcVegContext& ctx, Vec2i units)
		{
			const auto* pNode = GetNodeAtUnits(ctx, units);
			if (!pNode || !pNode->m_rangeInfo.pHMData)
				return 0.0f;

			const auto& ri = pNode->m_rangeInfo;
			int nMask = ri.nSize - 2;

			if (ri.nUnitBitShift == 0)
				return HeightLocal(ri, units.x & nMask, units.y & nMask);


			float fInvStep = (ri.nSize > 1) ? (1.f / ((1 << ctx.nUnitsToSectorBitShift) / (ri.nSize - 1))) : 1.f;

			int nX = units.x >> ri.nUnitBitShift;
			int nY = units.y >> ri.nUnitBitShift;

			float fX = (units.x * fInvStep) - nX;
			float fY = (units.y * fInvStep) - nY;

			nX &= nMask;
			nY &= nMask;

			return HeightLocal(ri, nX, nY) * (1.f - fX) * (1.f - fY) +
				HeightLocal(ri, nX + 1, nY) * fX * (1.f - fY) +
				HeightLocal(ri, nX, nY + 1) * (1.f - fX) * fY +
				HeightLocal(ri, nX + 1, nY + 1) * fX * fY;
		}

		[[nodiscard]] float GetTerrainZUnitsSafe(const SProcVegContext& ctx, Vec2i units)
		{
			units.x = std::clamp(units.x, 0, ctx.nTerrainUnits - 1);
			units.y = std::clamp(units.y, 0, ctx.nTerrainUnits - 1);

			GetTerrainZUnits(ctx, units);
		}

		[[nodiscard]] float GetTerrainZFast(const SProcVegContext& ctx, const Vec2& pos)
		{
			return GetTerrainZUnits(ctx, WorldToTerrainUnits(ctx, pos));
		}

		[[nodiscard]] float GetTerrainZApr(const SProcVegContext& ctx, const Vec2& pos)
		{
			const float uX = pos.x * ctx.fInvUnitSize;
			const float uY = pos.y * ctx.fInvUnitSize;

			const int nX = static_cast<int>(uX);
			const int nY = static_cast<int>(uY);

			if (!(pos.x > 0.0f && pos.y > 0.0f) || nX < 0 || nY < 0 || nX >= ctx.nTerrainUnits || nY >= ctx.nTerrainUnits)
				return kTerrainBottomLevel;

			const float fX = uX - nX;
			const float fY = uY - nY;

			const float z00 = GetTerrainZUnits(ctx, Vec2i(nX, nY ));
			const float z10 = GetTerrainZUnits(ctx, Vec2i(nX + 1, nY));
			const float z01 = GetTerrainZUnits(ctx, Vec2i(nX, nY + 1));

			if (fX + fY < 1.f)
				return z00 * (1.f - fX - fY) + z10 * fX + z01 * fY;

			const float z11 = GetTerrainZUnits(ctx, Vec2i(nX + 1, nY + 1));
			return z11 * (fX + fY - 1.f) + z01 * (1.f - fX) + z10 * (1.f - fY);
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

		[[nodiscard]] float GetSurfaceTypeAmountCustom(const SProcVegContext& ctx, const Vec3& pos, uint8 ucGlobalSurfType)
		{
			Vec2i units = WorldToTerrainUnits(ctx, pos);

			const auto* pNode = GetNodeAtUnits(ctx, units);
			if (!pNode)
				return 0.0f;

			const auto& ri = pNode->m_rangeInfo;
			if (!ri.pHMData || !ri.pSTPalette)
				return 0.0f;

			float fLocalUX = (pos.x - static_cast<float>(pNode->m_nOriginX)) * ctx.fInvUnitSize;
			float fLocalUY = (pos.y - static_cast<float>(pNode->m_nOriginY)) * ctx.fInvUnitSize;

			int lx1 = std::max(0, static_cast<int>(fLocalUX));
			int ly1 = std::max(0, static_cast<int>(fLocalUY));

			int nMaxIdx = static_cast<int>(ri.nSize) - 1;
			int lx2 = std::min(lx1 + 1, nMaxIdx);
			int ly2 = std::min(ly1 + 1, nMaxIdx);

			lx1 = std::min(lx1, nMaxIdx);
			ly1 = std::min(ly1, nMaxIdx);

			float fDX = fLocalUX - static_cast<float>(lx1);
			float fDY = fLocalUY - static_cast<float>(ly1);

			auto getGlobalType = [&ri](int lx, int ly) -> int
			{
				const uint32 idx = static_cast<uint32>(lx) * ri.nSize + static_cast<uint32>(ly);
				const uint32 localType = ri.pHMData[idx].surface & SRangeInfo_Mock::e_index_hole;
				return (localType < SRangeInfo_Mock::e_index_undefined) ? ri.pSTPalette[localType] : -1;
			};

			int t00 = getGlobalType(lx1, ly1);
			int t01 = getGlobalType(lx1, ly2);
			int t10 = getGlobalType(lx2, ly1);
			int t11 = getGlobalType(lx2, ly2);

			if (ctx.bHasSSE2)
				return GetSurfaceTypeAmountSSE2(t00, t01, t10, t11, ucGlobalSurfType, fDX, fDY);
			else
				return GetSurfaceTypeAmountScalar(t00, t01, t10, t11, ucGlobalSurfType, fDX, fDY);
		}

		[[nodiscard]] uint8 CalculateAngleByte(const IStatInstGroup& group, const Vec3& pos, uint32 angleRnd)
		{
			const int32 nRotationRange = group.nRotationRangeToTerrainNormal;

			if (group.bRandomRotation || nRotationRange >= 360)
				return static_cast<uint8>(angleRnd);

			if (nRotationRange > 0)
			{
				const Vec3 terrainNormal = gEnv->p3DEngine->GetTerrainSurfaceNormal(pos);

				if (terrainNormal.x == 0.0f && terrainNormal.y == 0.0f)
					return static_cast<uint8>(angleRnd);

				const float rndDegree = -static_cast<float>(nRotationRange) * 0.5f + static_cast<float>(angleRnd % static_cast<uint32>(nRotationRange));
				const float finalDegree = RAD2DEG(atan2f(terrainNormal.y, terrainNormal.x)) + rndDegree;

				return static_cast<uint8>(static_cast<int>((finalDegree / 360.0f) * 255.0f));
			}

			return 0;
		}

		[[nodiscard]] bool GenerateGroupInstances(const SProcVegContext& ctx, const SSurfaceType_Mock& surface, const SProcVegGroupParams& params, const Data::SBounds& bounds, CMTRand_int32& rndGen, bool bEmit, int& nInstancesCounter, std::vector<SVegetationInstanceData>& result)
		{
			for (float fX = bounds.fMinX + params.fOffset; fX < bounds.fMaxX; fX += params.fDensity)
			{
				for (float fY = bounds.fMinY + params.fOffset; fY < bounds.fMaxY; fY += params.fDensity)
				{
					const float fDrawFirst = rndGen.GenerateFloat();
					const float fDrawSecond = rndGen.GenerateFloat();
					const float fJitterX = kJitterYDrawnBeforeX ? fDrawSecond : fDrawFirst;
					const float fJitterY = kJitterYDrawnBeforeX ? fDrawFirst : fDrawSecond;

					Vec3 pos(fX + (fJitterX - 0.5f) * params.fDensity, fY + (fJitterY - 0.5f) * params.fDensity, 0.0f);
					pos.x = clamp_tpl(pos.x, bounds.fMinX, bounds.fMaxX);
					pos.y = clamp_tpl(pos.y, bounds.fMinY, bounds.fMaxY);

					float fSurfaceTypeAmount = GetSurfaceTypeAmountCustom(ctx, pos, surface.ucThisSurfaceTypeId);
					if (fSurfaceTypeAmount <= 0.5f)
						continue;

					pos.z = GetTerrainZApr(ctx, pos);

					if (pos.x < 0.f || pos.x >= ctx.fTerrainSize || pos.y < 0.f || pos.y >= ctx.fTerrainSize)
						continue;

					float fScale = params.group.fSize + (rndGen.GenerateFloat() - 0.5f) * params.group.fSizeVar;
					if (fScale <= 0.f)
						continue;

					if (pos.z < params.group.fElevationMin || pos.z > params.group.fElevationMax)
						continue;

					if (params.slope.bActive)
					{
						float sx = 0.0f, sy = 0.0f;

						if ((fX + ctx.fTerrainUnitSize) < ctx.fTerrainSize && fX >= ctx.fTerrainUnitSize)
							sx = GetTerrainZFast(ctx, { fX + ctx.fTerrainUnitSize, fY }) - GetTerrainZFast(ctx, { fX - ctx.fTerrainUnitSize, fY });
						if ((fY + ctx.fTerrainUnitSize) < ctx.fTerrainSize && fY >= ctx.fTerrainUnitSize)
							sy = GetTerrainZFast(ctx, { fX, fY + ctx.fTerrainUnitSize }) - GetTerrainZFast(ctx, { fX, fY - ctx.fTerrainUnitSize });

						float fGradSq = (sx * sx) + (sy * sy);
						if (!params.slope.Contains(fGradSq))
							continue;
					}

					if (params.group.fVegRadius * fScale < ctx.fVegMinSize)
						continue;

					// Keep the RNG sequence in sync with the original engine implementation.
					const uint32 angleRnd = rndGen.GenerateUint32();

					if (bEmit)
					{
						const float fAngle = static_cast<float>(CalculateAngleByte(params.group, pos, angleRnd)) * kAngleByteToRadians;
						result.emplace_back(pos, static_cast<uint16>(params.nGroupId), fScale, fAngle, params.layer);
					}

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

		void ProcessTerrainNode(const CTerrainNode_Mock& node, const SProcVegContext& ctx, const std::vector<MapLayers::EVegetationLayers>& idxToLayer, std::vector<SVegetationInstanceData>& result)
		{
			int nLastBakedPos = -1;
			{
				int nPos = 0;
				for (int nLayer = 0; nLayer < node.m_lstSurfaceTypeInfo.Count(); nLayer++)
				{
					const SSurfaceType_Mock* pSurface = node.m_lstSurfaceTypeInfo[nLayer].pSurfaceType;
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
			Data::SBounds nodeBounds = CreateProcVegBounds(node);

			int nInstancesCounter = 0;
			int nPos = 0;

			for (int nLayer = 0; nLayer < node.m_lstSurfaceTypeInfo.Count(); nLayer++)
			{
				SSurfaceType_Mock* pSurface = node.m_lstSurfaceTypeInfo[nLayer].pSurfaceType;
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
	}

	bool CanGenerateProcVeg() noexcept
	{
		return ValidateMockLayout();
	}

	void GenerateProcVeg(const std::vector<MapLayers::EVegetationLayers>& idxToLayer, std::vector<SVegetationInstanceData>& result)
	{
		ITerrain* pITerrain = gEnv->p3DEngine->GetITerrain();
		if (!pITerrain)
			return;

		const auto* pTerrain = reinterpret_cast<const CTerrain_Mock*>(pITerrain);
		if (pTerrain->m_arrSecInfoPyramid.m_nCount == 0 || !pTerrain->m_arrSecInfoPyramid.m_pElements)
			return;

		const auto& level0 = pTerrain->m_arrSecInfoPyramid.m_pElements[0];
		if (!level0.m_pData || level0.m_nSize <= 0)
			return;

		SProcVegContext ctx = SProcVegContext::Create(level0.m_pData, level0.m_nSize);

		const int levelsToProcess = std::min(pTerrain->m_arrSecInfoPyramid.m_nCount, ctx.nProcVegMaxCacheLevels);
		for (int level = 0; level < levelsToProcess; level++)
		{
			const auto& levelGrid = pTerrain->m_arrSecInfoPyramid.m_pElements[level];
			if (!levelGrid.m_pData || levelGrid.m_nSize <= 0)
				continue;

			const int gridSize = levelGrid.m_nSize;

			for (int i = 0; i < gridSize * gridSize; ++i)
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