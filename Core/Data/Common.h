#pragma once

namespace JDKLevelMaps::Data
{
	struct SBounds
	{
		float fMinX = 0.0f, fMaxX = 0.0f, fMinY = 0.0f, fMaxY = 0.0f;

		SBounds() = default;
		SBounds(float minX, float maxX, float minY, float maxY)
			: fMinX(minX), fMaxX(maxX), fMinY(minY), fMaxY(maxY) { }
	};
}