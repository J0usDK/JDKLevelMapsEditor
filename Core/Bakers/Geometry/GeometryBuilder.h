#pragma once
#include <memory>

#include <CryCore/BaseTypes.h>

struct IStatObj;

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	class CGroupGeometry;

	enum class EGeometryBuildMethod : uint8
	{
		BaseSlice = 0,
		Full
	};

	[[nodiscard]] std::shared_ptr<const CGroupGeometry> BuildGeometryFromStatObj(IStatObj* pObj, EGeometryBuildMethod method);

	[[nodiscard]] float CalculateHorizontalRadius(IStatObj* pObj) noexcept;
	[[nodiscard]] float CalculateMaxAbsHeight(IStatObj* pObj) noexcept;
}