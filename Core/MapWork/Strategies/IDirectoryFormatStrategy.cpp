#include "StdAfx.h"
#include "IDirectoryFormatStrategy.h"

#include "Core/Data/RunResult.h"
#include "Utils/VectorUtils.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	Data::SRunResult IDirectoryFormatStrategy::BuildRankTable() noexcept
	{
		try
		{
			m_rankTable.Build(m_bitmask);
			m_directoryInfo.nonEmptyTilesCount = m_rankTable.GetTotalRank();
			return { true, "" };
		}
		catch (...)
		{
			return { false, "Out of Memory: Failed to allocate directory ranks buffer" };
		}
	}

	std::optional<uint64> IDirectoryFormatStrategy::GetTileRank(uint64 tileIndex) const noexcept
	{
		const uint64 rank = m_rankTable.GetRank(tileIndex);
		if (rank == std::numeric_limits<uint64_t>::max())
			return std::nullopt;
		return rank;
	}

	uint64 IDirectoryFormatStrategy::GetTotalRank() const noexcept
	{
		return m_rankTable.GetTotalRank();
	}
}