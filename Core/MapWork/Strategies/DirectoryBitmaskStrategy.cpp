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
	Data::SRunResult CDirectoryBitmaskStrategy::WriteDirectory(FILE* pFile, const Data::SMapWriteContext& context, const void*)
	{
		FileSystem::LFSFacade::FSeek(pFile, context.directoryOffset, SEEK_SET);

		const size_t elementCount = context.bitmask.size();
		if (elementCount == 0)
			return { true, "" };

		const auto progressUpdate = [&context](size_t written)
			{ return context.pDirectoryTask ? context.pDirectoryTask->Update(written * sizeof(uint64)) : true; };

		if (!Utils::FileSystem::WriteDataChunked(pFile, context.bitmask.data(), sizeof(uint64), elementCount, progressUpdate))
			return { false, "Disk I/O Error: Cannot write directory bitmask" };
		return { true, "" };
	}

	Data::SRunResult CDirectoryBitmaskStrategy::ReadDirectory(FILE* pFile, const Data::SMapReadContext& context)
	{
		FileSystem::LFSFacade::FSeek(pFile, context.directoryOffset, SEEK_SET);

		const size_t totalTiles = static_cast<size_t>(context.header.tileCountX) * context.header.tileCountY;
		const size_t elementCount = static_cast<size_t>((totalTiles + 63) >> 6);

		if (!Utils::Common::TryResize(m_bitmask, elementCount))
			return { false, "Out of Memory: Failed to allocate directory bitmask buffer" };

		if (gEnv->pCryPak->FReadRaw(m_bitmask.data(), sizeof(uint64), elementCount, pFile) != elementCount)
			return { false, "Disk I/O Error: Failed to read directory bitmask" };

		m_directoryInfo.totalDirectorySize = elementCount * sizeof(uint64);
		m_baseDataOffset = context.directoryOffset + elementCount * sizeof(uint64);
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