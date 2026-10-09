#include "StdAfx.h"
#include "GeometryBuilder.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <limits>

#include <Cry3DEngine/IIndexedMesh.h>
#include <Cry3DEngine/IStatObj.h>

#include "GroupGeometry.h"
#include "Utils/VectorUtils.h"

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	namespace
	{
		constexpr float kBaseSliceRatio = 0.15f;
		constexpr float kBaseSliceMinHeight = 0.25f;

		void AppendMeshTriangles(IStatObj* pObj, const Matrix34& tm, std::vector<STriangle>& out, int depth)
		{
			if (!pObj || depth > 4)
				return;

			if (IIndexedMesh* pIndexed = pObj->GetIndexedMesh(true))
			{
				if (const CMesh* pMesh = pIndexed->GetMesh())
				{
					const int indexCount = pMesh->GetIndexCount();
					const bool bHasPos = pMesh->m_pPositions != nullptr;

					if (pMesh->m_pIndices && (bHasPos || pMesh->m_pPositionsF16))
					{
						const auto getPos = [&](int i) -> Vec3
						{
							const int v = pMesh->m_pIndices[i];
							return bHasPos ? pMesh->m_pPositions[v] : pMesh->m_pPositionsF16[v].ToVec3();
						};

						if (!Utils::Common::TryReserve(out, out.size() + indexCount / 3))
							return;

						for (int i = 0; i + 2 < indexCount; i += 3)
							out.emplace_back(tm * getPos(i), tm * getPos(i + 1), tm * getPos(i + 2));
					}
				}
			}

			const int subCount = pObj->GetSubObjectCount();
			for (int i = 0; i < subCount; ++i)
			{
				const IStatObj::SSubObject* pSub = pObj->GetSubObject(i);
				if (!pSub || pSub->nType != STATIC_SUB_OBJECT_MESH || !pSub->pStatObj || pSub->bHidden)
					continue;

				AppendMeshTriangles(pSub->pStatObj, tm * pSub->localTM, out, depth + 1);
			}
		}
	}

	std::shared_ptr<const CGroupGeometry> BuildGeometryFromStatObj(IStatObj* pObj, EGeometryBuildMethod method)
	{
		if (!pObj)
			return nullptr;

		std::vector<STriangle> triangles;
		AppendMeshTriangles(pObj, Matrix34(IDENTITY), triangles, 0);

		if (triangles.empty())
			return nullptr;

		if (method == EGeometryBuildMethod::BaseSlice)
		{
			float zMin = std::numeric_limits<float>::max();
			float zMax = std::numeric_limits<float>::lowest();

			for (const auto& tri : triangles)
			{
				for (const Vec3& v : tri.v)
				{
					zMin = std::min(zMin, v.z);
					zMax = std::max(zMax, v.z);
				}
			}

			const float height = zMax - zMin;
			const float sliceHeight = std::clamp(height * kBaseSliceRatio, kBaseSliceMinHeight, std::max(height, kBaseSliceMinHeight));

			std::vector<STriangle> slice;
			for (const auto& tri : triangles)
				ClipTriangleToSlab(tri, zMin, zMin + sliceHeight, slice);

			if (slice.empty())
				return nullptr;

			return std::make_shared<CGroupGeometry>(std::move(slice), true);
		}

		return std::make_shared<CGroupGeometry>(std::move(triangles), false);
	}

	float CalculateHorizontalRadius(IStatObj* pObj) noexcept
	{
		if (!pObj)
			return 0.0f;

		const AABB box = pObj->GetAABB();
		const float rx = std::max(std::fabs(box.min.x), std::fabs(box.max.x));
		const float ry = std::max(std::fabs(box.min.y), std::fabs(box.max.y));

		return std::sqrt(rx * rx + ry * ry);
	}

	float CalculateMaxAbsHeight(IStatObj* pObj) noexcept
	{
		if (!pObj)
			return 0.0f;

		const AABB box = pObj->GetAABB();
		return std::max(std::fabs(box.min.z), std::fabs(box.max.z));
	}
}