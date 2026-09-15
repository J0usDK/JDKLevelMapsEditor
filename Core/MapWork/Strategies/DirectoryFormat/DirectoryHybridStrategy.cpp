#include "StdAfx.h"
#include "DirectoryHybridStrategy.h"

#include "Core/Data/RunResult.h"
#include "Core/Data/MapContext.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/VectorUtils.h"
#include "Utils/Progress.h"
#include "Utils/IOUtils.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	template<typename TOffset>
	Data::SRunResult CDirectoryHybridStrategy<TOffset>::ParseDirectory(const std::vector<uint8>& directoryData, const Data::SMapReadContext& context)
	{
		const size_t totalTiles = static_cast<size_t>(context.header.tileCountX) * context.header.tileCountY;
		const size_t bitmaskElements = static_cast<size_t>((totalTiles + 63) >> 6);
		const size_t bitmaskBytes = bitmaskElements * sizeof(uint64);

		if (directoryData.size() < bitmaskBytes)
			return { false, "Directory Parse Error: Wrong data for bitmask" };

		if (!Utils::Common::TryResize(m_bitmask, bitmaskElements))
			return { false, "Out of Memory: Failed to allocate directory bitmask buffer" };

		std::memcpy(m_bitmask.data(), directoryData.data(), bitmaskBytes);

		if (auto result = BuildRankTable(); !result.bSuccess)
			return result;

		const size_t offsetsCount = static_cast<size_t>(m_directoryInfo.nonEmptyTilesCount) + 1;
		const size_t offsetsBytes = offsetsCount * sizeof(TOffset);

		if (directoryData.size() != bitmaskBytes + offsetsBytes)
			return { false, "Directory Parse Error: Wrong data for offsets" };

		if (!Utils::Common::TryResize(m_offsets, offsetsCount))
			return { false, "Out of Memory: Failed to allocate directory offsets buffer" };

		std::memcpy(m_offsets.data(), directoryData.data() + bitmaskBytes, offsetsBytes);

		m_directoryInfo.totalDirectorySize = bitmaskBytes + offsetsBytes;
		
		return { true, "" };
	}

	template<typename TOffset>
	Data::SDirectoryInfo CDirectoryHybridStrategy<TOffset>::GetDirectoryInfo() const noexcept
	{
		return m_directoryInfo;
	}

	template<typename TOffset>
	std::optional<Data::STileNode> CDirectoryHybridStrategy<TOffset>::GetTileNode(uint64 tileIndex) const noexcept
	{
		const std::optional<uint64> rank = GetTileRank(tileIndex);

		if (!rank.has_value())
			return std::nullopt;

		const uint64 offset = static_cast<uint64>(m_offsets[rank.value()]);
		const uint64 nextOffset = static_cast<uint64>(m_offsets[rank.value() + 1]);
		const uint64 tileSize = nextOffset - offset;

		return Data::STileNode{ offset, tileSize };
	}

	template class CDirectoryHybridStrategy<uint32>;
	template class CDirectoryHybridStrategy<uint64>;
}