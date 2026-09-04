#pragma once
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps::Data
{
	struct SLevelContext
	{
		uint32 tileSize = 0;
		int32 gridWidth = 0;
		int32 gridHeight = 0;
		float cellSize = 1.0f;
		float originX = 0.0f;
		float originY = 0.0f;
	};

	[[nodiscard]] int GetLevelTerrainSize() noexcept;

	[[nodiscard]] inline SLevelContext ComputeLevelContext(float cellSize, uint32 tileSize) noexcept
	{
		if (cellSize < 0.1f)
			cellSize = 0.1f;
		if (tileSize == 0)
			tileSize = 1;

		int terrainSize = GetLevelTerrainSize();
		CRY_ASSERT(terrainSize > 0, "[JDKLevelMaps] Invalid terrain size");

		int32 gridSize = static_cast<int32>((terrainSize / cellSize) + 0.5f);

		SLevelContext context;
		context.cellSize = cellSize;
		context.tileSize = tileSize;
		context.gridWidth = gridSize;
		context.gridHeight = gridSize;

		return context;
	}
}