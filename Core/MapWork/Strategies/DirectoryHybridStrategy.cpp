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
	Data::SRunResult CDirectoryHybridStrategy<TOffset>::WriteDirectory(FILE* pFile, const Data::SMapWriteContext& context, const void* pOffsets)
	{
		if (!pOffsets)
			return { false, "Internal Error: Offsets array is null in Hybrid Strategy" };

		FileSystem::LFSFacade::FSeek(pFile, context.directoryOffset, SEEK_SET);
		
		if (auto result = WriteBitmask(pFile, context); !result.bSuccess)
			return result;

		if (auto result = WriteOffsets(pFile, context, pOffsets); !result.bSuccess)
			return result;

		return { true, "" };
	}

	template<typename TOffset>
	Data::SRunResult CDirectoryHybridStrategy<TOffset>::WriteBitmask(FILE* pFile, const Data::SMapWriteContext& context)
	{
		const size_t bitmaskCount = context.bitmask.size();
		if (bitmaskCount == 0)
			return { true, "" };

		const auto progressUpdate = [&context](size_t written)
			{ return context.pDirectoryTask ? context.pDirectoryTask->Update(written * sizeof(uint64)) : true; };

		if (!Utils::FileSystem::WriteDataChunked(pFile, context.bitmask.data(), sizeof(uint64), bitmaskCount, progressUpdate))
			return { false, "Disk I/O Error: Cannot write directory bitmask" };

		return { true, "" };
	}

	template<typename TOffset>
	Data::SRunResult CDirectoryHybridStrategy<TOffset>::WriteOffsets(FILE* pFile, const Data::SMapWriteContext& context, const void* pOffsets)
	{
		const size_t offsetsCount = context.nonEmptyTilesCount + 1;
		const size_t bitmaskBytesWritten = context.bitmask.size() * sizeof(uint64);

		const auto updateProgress = [&context, bitmaskBytesWritten](size_t written)
			{ return context.pDirectoryTask ? context.pDirectoryTask->Update(bitmaskBytesWritten + (written * sizeof(TOffset))) : true; };

		if (!Utils::FileSystem::WriteDataChunked(pFile, pOffsets, sizeof(TOffset), offsetsCount, updateProgress))
			return { false, "Disk I/O Error: Cannot write tile offsets" };

		return { true, "" };
	}

	template<typename TOffset>
	Data::SRunResult CDirectoryHybridStrategy<TOffset>::ReadDirectory(FILE* pFile, const Data::SMapReadContext& context)
	{
		FileSystem::LFSFacade::FSeek(pFile, context.directoryOffset, SEEK_SET);

		const size_t totalTiles = static_cast<size_t>(context.header.tileCountX) * context.header.tileCountY;
		const size_t bitmaskElements = static_cast<size_t>((totalTiles + 63) >> 6);

		if (!Utils::Common::TryResize(m_bitmask, bitmaskElements))
			return { false, "Out of Memory: Failed to allocate directory bitmask buffer" };

		if (gEnv->pCryPak->FReadRaw(m_bitmask.data(), sizeof(uint64), bitmaskElements, pFile) != bitmaskElements)
			return { false, "Disk I/O Error: Failed to read directory bitmask" };

		if (auto result = BuildRankTable(); !result.bSuccess)
			return result;

		const size_t offsetsCount = static_cast<size_t>(m_directoryInfo.nonEmptyTilesCount) + 1;

		if (!Utils::Common::TryResize(m_offsets, offsetsCount))
			return { false, "Out of Memory: Failed to allocate directory offsets buffer" };

		if (gEnv->pCryPak->FReadRaw(m_offsets.data(), sizeof(TOffset), offsetsCount, pFile) != offsetsCount)
			return { false, "Disk I/O Error: Failed to read tile offsets" };

		m_directoryInfo.totalDirectorySize = (bitmaskElements * sizeof(uint64)) + (offsetsCount * sizeof(TOffset));

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