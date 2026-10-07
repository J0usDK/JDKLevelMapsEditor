#pragma once
#include <limits>
#include <vector>

#include <CryCore/BaseTypes.h>
#include <CryMath/Cry_Vector2.h>
#include <CryMath/Cry_Vector3.h>

#include "Core/Data/Common.h"

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	struct STriangle
	{
		Vec3 v[3];

		STriangle() = default;
		constexpr STriangle(const Vec3& a, const Vec3& b, const Vec3& c) noexcept
			: v{ a, b, c } { }
	};

	struct SFootprintParams
	{
		float zMin = std::numeric_limits<float>::lowest();
		float zMax = std::numeric_limits<float>::max();

		bool bFillEnclosedAreas = false;
	};

	class CFootprintMask final
	{
	public:
		CFootprintMask() = default;
		CFootprintMask(const Data::SBounds& bounds, int width, int height, const std::vector<uint8>& texels);

		[[nodiscard]] bool IsEmpty() const noexcept { return m_width == 0 || m_height == 0; }
		[[nodiscard]] const Data::SBounds& GetBounds() const noexcept { return m_bounds; }

		[[nodiscard]] float GetBoundingRadius() const noexcept { return m_boundingRadius; }
		
		[[nodiscard]] float SampleCoverage(const Vec2& center, float halfSize) const noexcept;

	private:
		Data::SBounds m_bounds;
		
		int m_width = 0;
		int m_height = 0;
		int m_stride = 0;

		float m_invTexelX = 0.0f;
		float m_invTexelY = 0.0f;
		float m_boundingRadius = 0.0f;

		std::vector<uint32> m_sat;

		bool m_bUseSSE41 = false;
	};

	[[nodiscard]] CFootprintMask BuildFootprintMask(const std::vector<STriangle>& triangles, const SFootprintParams& params);
}