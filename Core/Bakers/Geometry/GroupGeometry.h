#pragma once
#include <vector>

#include <CryCore/BaseTypes.h>
#include <CryMath/Cry_Vector3.h>

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	struct STriangle
	{
		Vec3 v[3];

		STriangle() = default;
		constexpr STriangle(const Vec3& a, const Vec3& b, const Vec3& c) noexcept
			: v{ a, b, c } { }
	};

	class CGroupGeometry final
	{
	public:
		CGroupGeometry(std::vector<STriangle>&& triangles, bool bFillEnclosed);

		[[nodiscard]] const std::vector<STriangle>& GetTriangles() const noexcept { return m_triangles; }
		[[nodiscard]] bool ShouldFillEnclosed() const noexcept { return m_bFillEnclosed; }
		[[nodiscard]] float GetBoundingRadius() const noexcept { return m_boundingRadius; }
		[[nodiscard]] float GetMaxAbsZ() const noexcept { return m_maxAbsZ; }

	private:
		std::vector<STriangle> m_triangles;

		float m_boundingRadius = 0.0f;
		float m_maxAbsZ = 0.0f;

		bool m_bFillEnclosed = false;
	};

	void ClipTriangleToSlab(const STriangle& tri, float zMin, float zMax, std::vector<STriangle>& out);
	void FillEnclosedAreas(std::vector<uint8>& texels, int width, int height);
}