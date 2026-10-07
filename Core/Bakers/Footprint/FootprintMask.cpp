#include "StdAfx.h"
#include "FootprintMask.h"

#include <algorithm>
#include <cmath>
#include <smmintrin.h>

#include <Includes/JDKMath/CpuFeatures.h>

#include "Utils/VectorUtils.h"

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	namespace
	{
		constexpr int kMaxResolution = 128;
		constexpr float kMinExtent = 0.001f;

		struct STriangle2D
		{
			Vec2 v[3];

			STriangle2D(const Vec3& a, const Vec3& b, const Vec3& c) noexcept
			{
				v[0] = Vec2(a.x, a.y);
				v[1] = Vec2(b.x, b.y);
				v[2] = Vec2(c.x, c.y);
			}
		};

		struct SGridDimensions
		{
			int width = 0;
			int height = 0;
			float texelX = 0.0f;
			float texelY = 0.0f;
			float invTexelX = 0.0f;
			float invTexelY = 0.0f;
		};

		struct STriangleOverlapCache
		{
			Vec2 normals[3];
			float triMin[3];
			float triMax[3];
			float boxRadius[3];

			STriangleOverlapCache(const STriangle2D& tri, const Vec2& extents) noexcept
			{
				for (int e = 0; e < 3; ++e)
				{
					const Vec2 edge = tri.v[(e + 1) % 3] - tri.v[e];
					normals[e] = Vec2(-edge.y, edge.x);

					const float p0 = normals[e].Dot(tri.v[0]);
					const float p1 = normals[e].Dot(tri.v[1]);
					const float p2 = normals[e].Dot(tri.v[2]);

					triMin[e] = std::min(p0, std::min(p1, p2));
					triMax[e] = std::max(p0, std::max(p1, p2));
					boxRadius[e] = extents.x * std::fabs(normals[e].x) + extents.y * std::fabs(normals[e].y);
				}
			}

			[[nodiscard]] bool Overlaps(const Vec2& boxCenter) const noexcept
			{
				for (int e = 0; e < 3; ++e)
				{
					const float pBox = normals[e].Dot(boxCenter);
					if (pBox + boxRadius[e] < triMin[e] || pBox - boxRadius[e] > triMax[e])
						return false;
				}

				return true;
			}
		};

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

		bool ClipTriangleZ(const STriangle& tri, float zMin, float zMax, std::vector<STriangle2D>& out)
		{
			const float zLo = std::min(tri.v[0].z, std::min(tri.v[1].z, tri.v[2].z));
			const float zHi = std::max(tri.v[0].z, std::max(tri.v[1].z, tri.v[2].z));

			if (zHi < zMin || zLo > zMax)
				return false;

			if (zLo >= zMin && zHi <= zMax)
				{ out.emplace_back(tri.v[0], tri.v[1], tri.v[2]); return true; }

			Vec3 bufA[8];
			Vec3 bufB[8];

			bufA[0] = tri.v[0];
			bufA[1] = tri.v[1];
			bufA[2] = tri.v[2];

			int count = ClipPolygon(bufA, 3, bufB, zMin, true);
			if (count < 3)
				return false;

			count = ClipPolygon(bufB, count, bufA, zMax, false);
			if (count < 3)
				return false;

			if (!Utils::Common::TryReserve(out, static_cast<size_t>(count - 2)))
				return false;

			for (int i = 1; i + 1 < count; ++i)
				out.emplace_back(bufA[0], bufA[i], bufA[i + 1]);

			return true;
		}

		[[nodiscard]] std::vector<STriangle2D> ExtractAndClipTriangles(const std::vector<STriangle>& triangles, const SFootprintParams& params)
		{
			std::vector<STriangle2D> tris;
			if (!Utils::Common::TryReserve(tris, triangles.size()))
				return tris;

			const bool bClip = params.zMin > std::numeric_limits<float>::lowest() || params.zMax < std::numeric_limits<float>::max();

			for (const STriangle& tri : triangles)
			{
				if (!bClip)
					tris.emplace_back(tri.v[0], tri.v[1], tri.v[2]);
				else
					ClipTriangleZ(tri, params.zMin, params.zMax, tris);
			}

			return tris;
		}

		[[nodiscard]] Data::SBounds Calculate2DBounds(const std::vector<STriangle2D>& tris) noexcept
		{
			float minX = std::numeric_limits<float>::max();
			float minY = std::numeric_limits<float>::max();
			float maxX = std::numeric_limits<float>::lowest();
			float maxY = std::numeric_limits<float>::lowest();

			for (const STriangle2D& tri : tris)
			{
				for (int i = 0; i < 3; ++i)
				{
					minX = std::min(minX, tri.v[i].x);
					maxX = std::max(maxX, tri.v[i].x);
					minY = std::min(minY, tri.v[i].y);
					maxY = std::max(maxY, tri.v[i].y);
				}
			}

			if (!(maxX - minX >= kMinExtent))
			{
				const float c = 0.5f * (minX + maxX);
				minX = c - 0.5f * kMinExtent;
				maxX = c + 0.5f * kMinExtent;
			}
			if (!(maxY - minY >= kMinExtent))
			{
				const float c = 0.5f * (minY + maxY);
				minY = c - 0.5f * kMinExtent;
				maxY = c + 0.5f * kMinExtent;
			}

			return Data::SBounds(minX, maxX, minY, maxY);
		}

		[[nodiscard]] SGridDimensions CalculateGridDimensions(const Data::SBounds& bounds)
		{
			const float	sizeX = bounds.fMaxX - bounds.fMinX;
			const float sizeY = bounds.fMaxY - bounds.fMinY;
			const float longest = std::max(sizeX, sizeY);

			SGridDimensions dim;
			dim.width = std::clamp(static_cast<int>(std::ceil(kMaxResolution * sizeX / longest)), 1, kMaxResolution);
			dim.height = std::clamp(static_cast<int>(std::ceil(kMaxResolution * sizeY / longest)), 1, kMaxResolution);

			dim.texelX = sizeX / static_cast<float>(dim.width);
			dim.texelY = sizeY / static_cast<float>(dim.height);
			dim.invTexelX = 1.0f / dim.texelX;
			dim.invTexelY = 1.0f / dim.texelY;

			return dim;
		}

		[[nodiscard]] std::vector<uint8> RasterizeTriangles(const std::vector<STriangle2D>& tris, const Data::SBounds& bounds, const SGridDimensions& dim)
		{
			std::vector<uint8> texels;
			if (!Utils::Common::TryAssign(texels, static_cast<size_t>(dim.width) * static_cast<size_t>(dim.height), 0))
				return texels;

			const Vec2 extents(0.5f * dim.texelX, 0.5f * dim.texelY);

			for (const STriangle2D& tri : tris)
			{
				const float tMinX = std::min(tri.v[0].x, std::min(tri.v[1].x, tri.v[2].x));
				const float tMaxX = std::max(tri.v[0].x, std::max(tri.v[1].x, tri.v[2].x));
				const float tMinY = std::min(tri.v[0].y, std::min(tri.v[1].y, tri.v[2].y));
				const float tMaxY = std::max(tri.v[0].y, std::max(tri.v[1].y, tri.v[2].y));

				const int i0 = std::clamp(static_cast<int>(std::floor((tMinX - bounds.fMinX) * dim.invTexelX)), 0, dim.width - 1);
				const int i1 = std::clamp(static_cast<int>(std::floor((tMaxX - bounds.fMinX) * dim.invTexelX)), 0, dim.width - 1);
				const int j0 = std::clamp(static_cast<int>(std::floor((tMinY - bounds.fMinY) * dim.invTexelY)), 0, dim.height - 1);
				const int j1 = std::clamp(static_cast<int>(std::floor((tMaxY - bounds.fMinY) * dim.invTexelY)), 0, dim.height - 1);

				const STriangleOverlapCache overlapCache(tri, extents);

				for (int j = j0; j <= j1; ++j)
				{
					const float boxMinY = bounds.fMinY + static_cast<float>(j) * dim.texelY;
					const float boxCenterY = boxMinY + extents.y;
					const size_t rowOffset = static_cast<size_t>(j) * dim.width;

					for (int i = i0; i <= i1; ++i)
					{
						uint8& texel = texels[rowOffset + i];
						if (texel)
							continue;

						const float boxMinX = bounds.fMinX + static_cast<float>(i) * dim.texelX;
						if (overlapCache.Overlaps(Vec2(boxMinX + extents.x, boxCenterY)))
							texel = 1;
					}
				}
			}

			return texels;
		}

		[[nodiscard]] float IntegralScalar(const Vec2& pos, int width, int height, int stride, const uint32* pSat) noexcept
		{
			const float fW = static_cast<float>(width);
			const float fH = static_cast<float>(height);

			const float x = std::clamp(pos.x, 0.0f, fW);
			const float y = std::clamp(pos.y, 0.0f, fH);

			const int ix = std::min(static_cast<int>(x), width - 1);
			const int iy = std::min(static_cast<int>(y), height - 1);
			const float fx = x - static_cast<float>(ix);
			const float fy = y - static_cast<float>(iy);

			const size_t safeStride = static_cast<size_t>(stride);
			const float s00 = static_cast<float>(pSat[iy * safeStride + ix]);
			const float s10 = static_cast<float>(pSat[iy * safeStride + ix + 1]);
			const float s01 = static_cast<float>(pSat[(iy + 1) * safeStride + ix]);
			const float s11 = static_cast<float>(pSat[(iy + 1) * safeStride + ix + 1]);

			const float top = s00 + (s10 - s00) * fx;
			const float bottom = s01 + (s11 - s01) * fx;

			return top + (bottom - top) * fy;
		}

		[[nodiscard]] __m128 IntegralSIMD(__m128 x_vec, __m128 y_vec, int width, int height, int stride, const uint32* pSat) noexcept
		{
			const __m128 zero = _mm_setzero_ps();
			const __m128 w_vec = _mm_set1_ps(static_cast<float>(width));
			const __m128 h_vec = _mm_set1_ps(static_cast<float>(height));

			x_vec = _mm_max_ps(zero, _mm_min_ps(x_vec, w_vec));
			y_vec = _mm_max_ps(zero, _mm_min_ps(y_vec, h_vec));

			__m128i ix_vec = _mm_cvttps_epi32(x_vec);
			__m128i iy_vec = _mm_cvttps_epi32(y_vec);

			const __m128i w_minus_1 = _mm_set1_epi32(width - 1);
			const __m128i h_minus_1 = _mm_set1_epi32(height - 1);
			ix_vec = _mm_min_epi32(ix_vec, w_minus_1);
			iy_vec = _mm_min_epi32(iy_vec, h_minus_1);

			const __m128 fx = _mm_sub_ps(x_vec, _mm_cvtepi32_ps(ix_vec));
			const __m128 fy = _mm_sub_ps(y_vec, _mm_cvtepi32_ps(iy_vec));

			const __m128i stride_vec = _mm_set1_epi32(stride);
			const __m128i idx_vec = _mm_add_epi32(_mm_mullo_epi32(iy_vec, stride_vec), ix_vec);

			alignas(16) int indices[4];
			_mm_store_si128(reinterpret_cast<__m128i*>(indices), idx_vec);

			const __m128 s00 = _mm_setr_ps(
				static_cast<float>(pSat[indices[0]]),
				static_cast<float>(pSat[indices[1]]),
				static_cast<float>(pSat[indices[2]]),
				static_cast<float>(pSat[indices[3]]));
			const __m128 s10 = _mm_setr_ps(
				static_cast<float>(pSat[indices[0] + 1]),
				static_cast<float>(pSat[indices[1] + 1]),
				static_cast<float>(pSat[indices[2] + 1]),
				static_cast<float>(pSat[indices[3] + 1])
			);
			const __m128 s01 = _mm_setr_ps(
				static_cast<float>(pSat[indices[0] + stride]),
				static_cast<float>(pSat[indices[1] + stride]),
				static_cast<float>(pSat[indices[2] + stride]),
				static_cast<float>(pSat[indices[3] + stride])
			);
			const __m128 s11 = _mm_setr_ps(
				static_cast<float>(pSat[indices[0] + stride + 1]),
				static_cast<float>(pSat[indices[1] + stride + 1]),
				static_cast<float>(pSat[indices[2] + stride + 1]),
				static_cast<float>(pSat[indices[3] + stride + 1])
			);

			const __m128 top = _mm_add_ps(s00, _mm_mul_ps(_mm_sub_ps(s10, s00), fx));
			const __m128 bottom = _mm_add_ps(s01, _mm_mul_ps(_mm_sub_ps(s11, s01), fx));
			return _mm_add_ps(top, _mm_mul_ps(_mm_sub_ps(bottom, top), fy));
		}
	}

	CFootprintMask::CFootprintMask(const Data::SBounds& bounds, int width, int height, const std::vector<uint8>& texels)
		: m_bounds(bounds), m_width(width), m_height(height)
	{
		if (width <= 0 || height <= 0 || texels.size() != static_cast<size_t>(width) * static_cast<size_t>(height))
		{
			m_width = 0;
			m_height = 0;
			m_stride = 0;
			return;
		}

		m_stride = width + 1;

		m_invTexelX = static_cast<float>(width) / (bounds.fMaxX - bounds.fMinX);
		m_invTexelY = static_cast<float>(height) / (bounds.fMaxY - bounds.fMinY);

		const float rx = std::max(std::fabs(bounds.fMinX), std::fabs(bounds.fMaxX));
		const float ry = std::max(std::fabs(bounds.fMinY), std::fabs(bounds.fMaxY));
		m_boundingRadius = std::sqrt(rx * rx + ry * ry);

		m_bUseSSE41 = JDK::Math::GetCpuFeatures().bHasSSE41;

		if (!Utils::Common::TryAssign(m_sat, static_cast<size_t>(m_stride) * (static_cast<size_t>(height) + 1), 0))
		{
			m_width = 0;
			m_height = 0;
			m_stride = 0;
			return;
		}

		for (int y = 0; y < height; ++y)
		{
			uint32 rowSum = 0;
			const uint8* pTexelRow = &texels[static_cast<size_t>(y) * width];
			uint32* pSatRow = &m_sat[static_cast<size_t>(y) * m_stride];
			uint32* pSatNextRow = &m_sat[(static_cast<size_t>(y) + 1) * m_stride];

			for (int x = 0; x < width; ++x)
			{
				rowSum += pTexelRow[x];
				pSatNextRow[x + 1] = pSatRow[x + 1] + rowSum;
			}
		}
	}

	float CFootprintMask::SampleCoverage(const Vec2& center, float halfSize) const noexcept
	{
		if (IsEmpty())
			return 0.0f;

		const float x0 = (center.x - halfSize - m_bounds.fMinX) * m_invTexelX;
		const float x1 = (center.x + halfSize - m_bounds.fMinX) * m_invTexelX;
		const float y0 = (center.y - halfSize - m_bounds.fMinY) * m_invTexelY;
		const float y1 = (center.y + halfSize - m_bounds.fMinY) * m_invTexelY;

		if (x1 <= 0.0f || y1 <= 0.0f || x0 >= static_cast<float>(m_width) || y0 >= static_cast<float>(m_height))
			return 0.0f;

		const float area = (x1 - x0) * (y1 - y0);
		if (!(area > 0.0f))
			return 0.0f;

		float sum = 0.0f;
		if (m_bUseSSE41)
		{
			const __m128 x_vec = _mm_setr_ps(x1, x0, x1, x0);
			const __m128 y_vec = _mm_setr_ps(y1, y1, y0, y0);

			const __m128 res_vec = IntegralSIMD(x_vec, y_vec, m_width, m_height, m_stride, m_sat.data());

			alignas(16) float res[4];
			_mm_store_ps(res, res_vec);

			sum = res[0] - res[1] - res[2] + res[3];
		}
		else
		{
			sum = IntegralScalar({ x1, y1 }, m_width, m_height, m_stride, m_sat.data()) -
				IntegralScalar({ x0, y1 }, m_width, m_height, m_stride, m_sat.data()) -
				IntegralScalar({ x1, y0 }, m_width, m_height, m_stride, m_sat.data()) +
				IntegralScalar({ x0, y0 }, m_width, m_height, m_stride, m_sat.data());
		}

		return std::clamp(sum / area, 0.0f, 1.0f);
	}

	CFootprintMask BuildFootprintMask(const std::vector<STriangle>& triangles, const SFootprintParams& params)
	{
		const auto tris2D = ExtractAndClipTriangles(triangles, params);
		if (tris2D.empty())
			return CFootprintMask();

		const auto bounds = Calculate2DBounds(tris2D);
		const auto dim = CalculateGridDimensions(bounds);

		auto texels = RasterizeTriangles(tris2D, bounds, dim);
		if (texels.empty())
			return CFootprintMask();

		if (params.bFillEnclosedAreas)
			FillEnclosedAreas(texels, dim.width, dim.height);

		return CFootprintMask(bounds, dim.width, dim.height, texels);
	}
}