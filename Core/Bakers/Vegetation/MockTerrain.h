#pragma once

#include <Cry3DEngine/IRenderNode.h>
#include <CrySystem/IStreamEngine.h>
#include <CryCore/Containers/CryArray.h>

namespace JDKLevelMaps::JDKEditorSource::Internal
{
	struct Cry3DEngineBase_Mock {};

	struct SSurfaceType_Mock
	{
		char			szName[128];
		void*			pLayerMat;
		float			fScale;
		PodArray<int>	lstnVegetationGroups;
		float			fMaxMatDistanceXY;
		float			fMaxMatDistanceZ;
		float			arrRECustomData[4][16];
		uint8			ucDefProjAxis;
		uint8			ucThisSurfaceTypeId;
	};

	struct SSurfaceTypeInfo_Mock
	{
		struct SSurfaceType_Mock* pSurfaceType;
		void*					  arrpRM[3];
	};

	struct SSurfaceTypeLocal_Mock
	{
		enum { kMaxSurfaceTypesNum = 3, kMaxSurfaceTypeId = 15, kMaxVal = 15 };

		uint8 ty[kMaxSurfaceTypesNum];
		uint8 we[kMaxSurfaceTypesNum];

		uint32 GetDominatingSurfaceType() const { return ty[0]; }

		static void DecodeFromUint32(const uint32& rTypes, SSurfaceTypeLocal_Mock& si)
		{
			uint8* p = (uint8*)&rTypes;
			si.ty[0] = (((int)p[0])) & kMaxVal;
			si.ty[1] = (((int)p[0]) >> 4) & kMaxVal;
			si.ty[2] = (((int)p[1])) & kMaxVal;
			si.we[1] = (((int)p[1]) >> 4) & kMaxVal;
			si.we[2] = (((int)p[2])) & kMaxVal;

			int w0 = kMaxVal - si.we[1] - si.we[2];
			si.we[0] = (uint8)(w0 < 0 ? 0 : (w0 > kMaxVal ? kMaxVal : w0));
		}
	};

	union SHeightMapItem_Mock
	{
		static const unsigned int SurfaceBits = 20;
		static const unsigned int HeightBits = 12;

		uint32 raw;
		struct
		{
			uint32 surface : SurfaceBits;
			uint32 height : HeightBits;
		};
	};

	struct SRangeInfo_Mock
	{
		enum
		{
			e_index_undefined = 14,
			e_index_hole = 15,
			e_palette_size = 16,
			e_undefined = 127,
			e_hole = 128,
			e_max_surface_types = 129
		};

		float					fOffset;
		float					fRange;
		SHeightMapItem_Mock* pHMData;

		uint16					nSize;
		uint8					nUnitBitShift;
		uint8					nModified;
		unsigned char* pSTPalette;

		inline uint32 GetSurfaceType(uint32 x, uint32 y) const
		{
			uint32 i = x * nSize + y;
			SSurfaceTypeLocal_Mock si;
			SSurfaceTypeLocal_Mock::DecodeFromUint32(pHMData[i].surface, si);
			return si.GetDominatingSurfaceType() & e_index_hole;
		}
	};

	class CTerrainNode_Mock final : public Cry3DEngineBase_Mock, public IRenderNode, public IStreamCallback
	{
	public:
		void*				m_pReadStream;
		int					m_eTexStreamingStatus;

		CTerrainNode_Mock* m_pChilds;

		uint8				m_bProcObjectsReady : 1;
		uint8				m_bHasHoles : 2;
		uint8				m_bNoOcclusion : 1;

#ifndef _RELEASE
		unsigned int		m_eTextureEditingState, m_eElevTexEditingState;
#endif

		uint8				m_cNodeNewTexMML, m_cNodeNewTexMML_Min;
		uint8				m_nTreeLevel;

		uint16				m_nOriginX, m_nOriginY;
		int					m_nLastTimeUsed;
		int					m_nSetLodFrameId;
		float				m_geomError;

	protected:
		void* m_pUpdateTerrainTempData;

	public:
		PodArray<SSurfaceTypeInfo_Mock> m_lstSurfaceTypeInfo;
		SRangeInfo_Mock					m_rangeInfo;
	};

	template<class T>
	struct Array2d_Mock
	{
		T* m_pData;
		int m_nSize;
	};

	template<class T, size_t overAllocBytes = 0>
	class PodArray_Mock
	{
	public:
		T* m_pElements;
		int m_nCount;
		int m_nAllocatedCount;
	};

	struct ITerrain_Mock
	{
		virtual ~ITerrain_Mock() {}
	};

	class CTerrain_Mock : public ITerrain_Mock, public Cry3DEngineBase_Mock
	{
	public:
		int m_nWhiteTexId;
		int m_nBlackTexId;

		PodArray_Mock<Array2d_Mock<CTerrainNode_Mock*>> m_arrSecInfoPyramid;
	};

	[[nodiscard]] static bool ValidateMockLayout()
	{
		ITerrain* pITerrain = gEnv->p3DEngine->GetITerrain();
		const auto* pTerrain = reinterpret_cast<const Internal::CTerrain_Mock*>(pITerrain);

		if (pTerrain->m_arrSecInfoPyramid.m_nCount <= 0)
			return false;

		const auto& level0 = pTerrain->m_arrSecInfoPyramid.m_pElements[0];
		const int nExpectedGridSize = gEnv->p3DEngine->GetTerrainSize() / gEnv->p3DEngine->GetTerrainSectorSize();

		if (level0.m_nSize != nExpectedGridSize)
			return false;

		for (int i = 0; i < level0.m_nSize * level0.m_nSize; i++)
		{
			if (const auto* pNode = level0.m_pData[i])
			{
				if (pNode->m_nOriginX >= gEnv->p3DEngine->GetTerrainSize() ||
					pNode->m_nOriginY >= gEnv->p3DEngine->GetTerrainSize())
					return false;
				break;
			}
		}

		return true;
	}
}