#include "StdAfx.h"
#include "GroupGeometry.h"

#include "Utils/VectorUtils.h"

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	namespace
	{
		[[nodiscard]] int ClipPolygon(const Vec3* pIn, int count, Vec3* pOut, float planeZ, bool bKeepAbove) noexcept
		{
			int outCount = 0;
			for (int i = 0; i < count; ++i)
			{
				const Vec3& a = pIn[i];
				const Vec3& b = pIn[(i + 1) % count];

				const float da = bKeepAbove ? a.z - planeZ : planeZ - a.z;
				const float db = bKeepAbove ? b.z - planeZ : planeZ - b.z;

				if (da >= 0.0f)
					pOut[outCount++] = a;
				if ((da >= 0.0f) != (db >= 0.0f))
					pOut[outCount++] = a + (b - a) * (da / (da - db));
			}

			return outCount;
		}
	}

	CGroupGeometry::CGroupGeometry(std::vector<STriangle>&& triangles, bool bFillEnclosed) : m_triangles(std::move(triangles)), m_bFillEnclosed(bFillEnclosed)
	{
		float maxRadiusSq = 0.0f;
		float maxAbsZ = 0.0f;

		for (const STriangle& tri : m_triangles)
		{
			for (const Vec3& v : tri.v)
			{
				maxRadiusSq = std::max(maxRadiusSq, v.x * v.x + v.y * v.y);
				maxAbsZ = std::max(maxAbsZ, std::fabs(v.z));
			}
		}

		m_boundingRadius = std::sqrt(maxRadiusSq);
		m_maxAbsZ = maxAbsZ;
	}

	void ClipTriangleToSlab(const STriangle& tri, float zMin, float zMax, std::vector<STriangle>& out)
	{
		const float zLo = std::min(tri.v[0].z, std::min(tri.v[1].z, tri.v[2].z));
		const float zHi = std::max(tri.v[0].z, std::max(tri.v[1].z, tri.v[2].z));

		if (zHi < zMin || zLo > zMax)
			return;

		if (zLo >= zMin && zHi <= zMax)
		{
			out.push_back(tri);
			return;
		}

		Vec3 bufA[8];
		Vec3 bufB[8];

		bufA[0] = tri.v[0];
		bufA[1] = tri.v[1];
		bufA[2] = tri.v[2];

		int count = ClipPolygon(bufA, 3, bufB, zMin, true);
		if (count < 3)
			return;

		count = ClipPolygon(bufB, count, bufA, zMax, false);
		if (count < 3)
			return;

		if (!Utils::Common::TryReserve(out, out.size() + static_cast<size_t>(count - 2)))
			return;

		for (int i = 1; i + 1 < count; ++i)
			out.emplace_back(bufA[0], bufA[i], bufA[i + 1]);
	}

	void FillEnclosedAreas(std::vector<uint8>& texels, int width, int height)
	{
		std::vector<int> stack;
		if (!Utils::Common::TryReserve(stack, texels.size()))
			return;

		const auto push = [&](int x, int y)
		{
			const int idx = y * width + x;
			if (texels[idx] == 0)
			{
				texels[idx] = 2;
				stack.push_back(idx);
			}
		};

		for (int x = 0; x < width; ++x)
		{
			push(x, 0);
			push(x, height - 1);
		}

		for (int y = 0; y < height; ++y)
		{
			push(0, y);
			push(width - 1, y);
		}

		while (!stack.empty())
		{
			const int idx = stack.back();
			stack.pop_back();

			const int x = idx % width;
			const int y = idx / width;

			if (x > 0)			push(x - 1, y);
			if (x < width - 1)	push(x + 1, y);
			if (y > 0)			push(x, y - 1);
			if (y < height - 1) push(x, y + 1);
		}

		for (uint8& texel : texels)
		{
			if (texel == 0)			texel = 1;
			else if (texel == 2)	texel = 0;
		}
	}
}