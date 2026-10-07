#pragma once
#include <memory>

#include <CryCore/BaseTypes.h>

struct IStatObj;

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	class CFootprintMask;

	enum class EFootprintBuildMethod : uint8
	{
		BaseSlice = 0,
		Full
	};

	[[nodiscard]] std::shared_ptr<const CFootprintMask> BuildMaskFromStatObj(IStatObj* pObj, EFootprintBuildMethod method);

	[[nodiscard]] float CalculateHorizontalRadius(IStatObj* pObj) noexcept;
}