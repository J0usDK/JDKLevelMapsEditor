#include "StdAfx.h"
#include "DirectoryBitmaskStrategy.h"

#include "Core/Data/RunResult.h"
#include "Core/Data/MapContext.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/VectorUtils.h"
#include "Utils/IOUtils.h"
#include "Utils/Progress.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	Data::SRunResult CDirectoryBitmaskStrategy::ParseDirectory(const std::vector<uint8>& directoryData, const Data::SMapReadContext& context)
	{
		const size_t totalTiles = static_cast<size_t>(context.header.tileCountX) * context.header.tileCountY;
		const size_t bitmaskElements = static_cast<size_t>((totalTiles + 63) >> 6);
		const size_t expectedBytes = bitmaskElements * sizeof(uint64);

		if (directoryData.size() != expectedBytes)
			return { false, "Directory Parse Error: wrong data for bitmask" };

		if (!Utils::Common::TryResize(m_bitmask, bitmaskElements))
			return { false, "Out of Memory: Failed to allocate directory bitmask buffer" };

		std::memcpy(m_bitmask.data(), directoryData.data(), expectedBytes);

		m_directoryInfo.totalDirectorySize = expectedBytes;
		m_baseDataOffset = sizeof(JDKLevelMaps::SMapHeader);
		m_tileSizeBytes = static_cast<size_t>(context.header.tileSize) * context.header.tileSize * context.channelsCount;

		return BuildRankTable();
	}

	Data::SDirectoryInfo CDirectoryBitmaskStrategy::GetDirectoryInfo() const noexcept
	{
		return m_directoryInfo;
	}

	std::optional<Data::STileNode> CDirectoryBitmaskStrategy::GetTileNode(uint64 tileIndex) const noexcept
	{
		const std::optional<uint64> rank = GetTileRank(tileIndex);

		if (!rank.has_value())
			return std::nullopt;

		return Data::STileNode{ m_baseDataOffset + (rank.value() * m_tileSizeBytes), m_tileSizeBytes };
	}
}