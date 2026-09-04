#include "StdAfx.h"
#include "MapFileWriter.h"

#include "Core/Data/RunResult.h"
#include "Core/Data/LevelContext.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/Progress.h"
#include "Utils/VectorUtils.h"
#include "Utils/IOUtils.h"
#include "Shared/MapHeader.h"

namespace
{
	void ExtractTileData(
		const std::vector<uint8>& flatData,
		uint32 tileX, uint32 tileY,
		const JDKLevelMaps::Data::SLevelContext& context,
		uint32 numChannels,
		std::vector<uint8>& outTileBuffer) noexcept
	{
		const uint32 tileStartX = tileX * context.tileSize;
		const uint32 tileStartY = tileY * context.tileSize;
		const uint32 maxLx = std::min(context.tileSize, static_cast<uint32>(context.gridWidth) - tileStartX);
		const uint32 maxLy = std::min(context.tileSize, static_cast<uint32>(context.gridHeight) - tileStartY);

		if (maxLx < context.tileSize || maxLy < context.tileSize)
			std::memset(outTileBuffer.data(), 0, outTileBuffer.size());

		const size_t bytesPerRow = static_cast<size_t>(maxLx) * numChannels;
		const size_t localStride = static_cast<size_t>(context.tileSize) * numChannels;
		const size_t globalStride = static_cast<size_t>(context.gridWidth) * numChannels;

		size_t globalStart = (static_cast<size_t>(tileStartY) * context.gridWidth + tileStartX) * numChannels;
		size_t localStart = 0;

		for (uint32 ly = 0; ly < maxLy; ++ly)
		{
			std::memcpy(&outTileBuffer[localStart], &flatData[globalStart], bytesPerRow);
			globalStart += globalStride;
			localStart += localStride;
		}
	}
}

namespace JDKLevelMaps::MapWork
{
	bool CMapFileWriter::WriteHeader(const Data::SLevelContext& context, FILE* pFile)
	{
		SMapHeader header;
		header.mapType = m_bakeContext.mapType;
		header.entryFormat = m_bakeContext.entryFormat;
		header.activeLayersMask = m_bakeContext.layersMask;
		header.gridWidth = context.gridWidth;
		header.gridHeight = context.gridHeight;
		header.cellSize = context.cellSize;
		header.originX = context.originX;
		header.originY = context.originY;
		header.tileSize = context.tileSize;
		header.tileCountX = m_bakeContext.tileCountX;
		header.tileCountY = m_bakeContext.tileCountY;

		return gEnv->pCryPak->FWrite(&header, sizeof(JDKLevelMaps::SMapHeader), 1, pFile) == 1;
	}

	Data::SRunResult CMapFileWriter::WriteMap(const Data::SLevelContext& context, FILE* pFile, BakeData bakingData)
	{
		if (auto result = ProcessTilesLoop(context, pFile, bakingData, [](uint64, uint64) {}); !result.bSuccess)
			return result;

		Data::SRunResult result = std::visit([&](auto&& strategy) -> Data::SRunResult
		{
			using T = std::decay_t<decltype(strategy)>;
			if constexpr (std::is_same_v<T, std::monostate>)
				return { false, "Internal Error: Strategy is not initialized" };
			else
				return strategy.WriteDirectory(pFile, m_bakeContext, nullptr);
		}, m_strategy);

		return result;
	}

	template<typename TOffset>
	Data::SRunResult CMapFileWriter::WriteMap(const Data::SLevelContext& context, FILE* pFile, BakeData bakingData)
	{
		std::vector<TOffset> offsets;

		if (!Utils::Common::TryResize(offsets, m_bakeContext.nonEmptyTilesCount + 1))
			return { false, "Out of Memory: Failed to allocate memory for offsets buffer" };

		const auto writeOffset = [&](uint64 index, uint64 offset)
			{ offsets[index] = static_cast<TOffset>(offset); };

		if (auto result = ProcessTilesLoop(context, pFile, bakingData, writeOffset); !result.bSuccess)
			return result;

		Data::SRunResult result = std::visit([&](auto&& strategy) -> Data::SRunResult
		{
			using T = std::decay_t<decltype(strategy)>;
			if constexpr (std::is_same_v<T, std::monostate>)
				return { false, "Internal Error: Strategy is not initialized" };
			else
				return strategy.WriteDirectory(pFile, m_bakeContext, offsets.data());
		}, m_strategy);

		return result;
	}

	template<typename TOffsetRecorder>
	Data::SRunResult CMapFileWriter::ProcessTilesLoop(const Data::SLevelContext& context, FILE* pFile, BakeData bakingData, TOffsetRecorder&& recordOffset)
	{
		const size_t tileBufferSize = static_cast<size_t>(context.tileSize) * context.tileSize * m_bakeContext.channelsCount;
		std::vector<uint8> tileBuffer;

		if (!Utils::Common::TryResize(tileBuffer, tileBufferSize))
			return { false, "Out of Memory: Failed to allocate memory for tile buffer" };

		uint32 tx = 0, ty = 0;
		uint64 nonEmptyProcessed = 0;
		uint64 bytesProcessed = 0;

		const auto updateProgress = [&](size_t written)
			{ return m_bakeContext.pTilesTask ? m_bakeContext.pTilesTask->Update(static_cast<double>(bytesProcessed + written)) : true; };

		FileSystem::LFSFacade::FSeek(pFile, m_bakeContext.tilesOffset, SEEK_SET);

		for (size_t tileIndex = 0; tileIndex < m_bakeContext.totalTiles; ++tileIndex)
		{
			const size_t blockIndex = tileIndex >> 6;
			const uint64 bitFlag = 1ULL << (tileIndex & 63);

			if ((m_bakeContext.bitmask[blockIndex] & bitFlag) != 0)
			{
				ExtractTileData(bakingData, tx, ty, context, m_bakeContext.channelsCount, tileBuffer);

				const uint64 currentOffset = JDKLevelMaps::FileSystem::LFSFacade::FTell(pFile);
				recordOffset(nonEmptyProcessed, currentOffset);

				if (!Utils::FileSystem::WriteDataChunked(pFile, tileBuffer.data(), 1, tileBuffer.size(), updateProgress))
					return { false, "Disk I/O Error: Failed to write tile data at index: " + std::to_string(tileIndex) };

				bytesProcessed += tileBuffer.size();
				nonEmptyProcessed++;
			}

			if (++tx == m_bakeContext.tileCountX)
				{ tx = 0; ++ty; }
		}

		const uint64 finalOffset = JDKLevelMaps::FileSystem::LFSFacade::FTell(pFile);
		recordOffset(nonEmptyProcessed, finalOffset);

		return { true, "" };
	}
}