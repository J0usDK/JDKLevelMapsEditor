#include "StdAfx.h"
#include "MapFileWriter.h"

#include "Core/Data/RunResult.h"
#include "Core/Data/LevelContext.h"
#include "Core/Bakers/IMapBaker.h"
#include "Core/FileSystem/PathResolver.h"
#include "Utils/VectorUtils.h"
#include "Utils/Progress.h"

namespace
{
	inline bool PixelHasData(const uint8* pPixelData, uint32 numChannels) noexcept
	{
		for (uint32 c = 0; c < numChannels; ++c)
			if (pPixelData[c] != 0) return true;
		return false;
	}
}

namespace JDKLevelMaps::MapWork
{
	Data::SRunResult CMapFileWriter::BuildBakeContext(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, const Data::SLevelContext& context, BakeData bakingData, bool bUseHybrid)
	{
		m_bakeContext.channelsCount = baker.GetChannelCount();
		m_bakeContext.layersMask = baker.GetActiveLayersMask();

		if (auto result = ValidateBakeContext(context); !result.bSuccess)
			return result;

		InitTileLayout(context);

		if (auto result = ComputeTilesOccupancy(context, bakingData); !result.bSuccess)
			return result;

		InitFileLayout(context, baker.GetMapType(), bUseHybrid);

		if (auto result = InitDirectoryStrategy(); !result.bSuccess)
			return result;

		if (auto result = ResolveMapPath(pathResolver, baker); !result.bSuccess)
			return result;

		return { true, "" };
	}

	Data::SRunResult CMapFileWriter::ValidateBakeContext(const Data::SLevelContext& context) const noexcept
	{
		if (context.gridWidth <= 0 || context.gridHeight <= 0)
			return { false, "Cannot bake: Level grid dimensions are invalid (Zero terrain size?)" };
		if (context.tileSize == 0)
			return { false, "Cannot bake: Tile size must be greater than zero" };
		if (m_bakeContext.channelsCount == 0)
			return { false, "Cannot bake: Baker returned zero channels" };
		return { true, "" };
	}

	void CMapFileWriter::InitTileLayout(const Data::SLevelContext& context) noexcept
	{
		m_bakeContext.tileCountX = (static_cast<uint32>(context.gridWidth) + context.tileSize - 1) / context.tileSize;
		m_bakeContext.tileCountY = (static_cast<uint32>(context.gridHeight) + context.tileSize - 1) / context.tileSize;
		m_bakeContext.totalTiles = static_cast<uint64>(m_bakeContext.tileCountX) * m_bakeContext.tileCountY;
	}

	void CMapFileWriter::InitFileLayout(const Data::SLevelContext& context, EMapType mapType, bool bUseHybrid) noexcept
	{
		m_bakeContext.directoryOffset = sizeof(SMapHeader);
		m_bakeContext.mapType = mapType;
		m_bakeContext.entryFormat = DetermineEntryFormat(context, bUseHybrid);
		m_bakeContext.tilesOffset = CalculateDataOffset(m_bakeContext.entryFormat, m_bakeContext.nonEmptyTilesCount);
	}

	Data::SRunResult CMapFileWriter::ComputeTilesOccupancy(const Data::SLevelContext& context, BakeData bakingData)
	{
		const size_t bitmaskSize = (m_bakeContext.totalTiles + 63) / 64;
		if (!Utils::Common::TryAssign(m_bakeContext.bitmask, bitmaskSize, 0))
			return { false, "Out of Memory: Failed to allocate tile occupancy data" };

		m_bakeContext.nonEmptyTilesCount = 0;
		for (int32 y = 0; y < context.gridHeight; ++y)
		{
			const uint32 ty = y / context.tileSize;
			const size_t rowTileOffset = static_cast<size_t>(ty) * m_bakeContext.tileCountX;
			const uint8* pRow = bakingData.data() + static_cast<size_t>(y) * context.gridWidth * m_bakeContext.channelsCount;

			uint tx = 0;
			uint32 xInTile = 0;

			for (int32 x = 0; x < context.gridWidth; ++x)
			{
				const size_t tileIndex = rowTileOffset + tx;
				const size_t blockIndex = tileIndex / 64;
				const uint64 bitFlag = 1ULL << (tileIndex % 64);

				if ((m_bakeContext.bitmask[blockIndex] & bitFlag) == 0 && PixelHasData(pRow, m_bakeContext.channelsCount))
				{
					m_bakeContext.bitmask[blockIndex] |= bitFlag;
					m_bakeContext.nonEmptyTilesCount++;
				}

				pRow += m_bakeContext.channelsCount;
				if (++xInTile == context.tileSize)
					{ tx++; xInTile = 0; }
			}
		}
		return { true, "" };
	}

	ETileEntryFormat CMapFileWriter::DetermineEntryFormat(const Data::SLevelContext& context, bool bUseHybrid) const noexcept
	{
		if (!bUseHybrid)
			return ETileEntryFormat::Bitmask;

		const uint64 size32 = CalculateSerializedSize(context, sizeof(uint32));

		return size32 <= UINT32_MAX ? ETileEntryFormat::Hybrid_32 : ETileEntryFormat::Hybrid_64;
	}

	uint64 CMapFileWriter::CalculateSerializedSize(const Data::SLevelContext& context, uint64 offsetSize) const noexcept
	{
		const uint64 bitmaskBytes = m_bakeContext.bitmask.size() * sizeof(uint64);
		const uint64 offsetsCount = m_bakeContext.nonEmptyTilesCount + 1;
		const uint64 fullTileByteSize = static_cast<uint64>(context.tileSize) * context.tileSize * m_bakeContext.channelsCount;

		return sizeof(SMapHeader) + bitmaskBytes + offsetsCount * offsetSize + m_bakeContext.nonEmptyTilesCount * fullTileByteSize;
	}

	uint64 CMapFileWriter::CalculateDataOffset(ETileEntryFormat format, uint64 nonEmptyTilesCount) const noexcept
	{
		const uint64 bitmaskBytes = m_bakeContext.bitmask.size() * sizeof(uint64_t);
		uint64 offsetBytes = 0;

		switch (format)
		{
		case ETileEntryFormat::Bitmask: break;
		case ETileEntryFormat::Hybrid_32:
			offsetBytes = (nonEmptyTilesCount + 1) * sizeof(uint32);
			break;
		case ETileEntryFormat::Hybrid_64:
			offsetBytes = (nonEmptyTilesCount + 1) * sizeof(uint64);
			break;
		}

		return sizeof(SMapHeader) + bitmaskBytes + offsetBytes;
	}

	Data::SRunResult CMapFileWriter::ResolveMapPath(FileSystem::CPathResolver& pathResolver, const Bakers::IMapBaker& baker)
	{
		if (auto resultPath = pathResolver.GetMapPath(baker.GetID()))
			m_bakeContext.mapPath = std::move(*resultPath);
		else
			return { false, "Disk I/O Error: Cannot get map's path" };
		return { true, "" };
	}

	Data::SRunResult CMapFileWriter::InitDirectoryStrategy() noexcept
	{
		switch (m_bakeContext.entryFormat)
		{
		case ETileEntryFormat::Bitmask:
			m_strategy.emplace<TBitmaskStrategy>();
			return { true, "" };
		case ETileEntryFormat::Hybrid_32:
			m_strategy.emplace<THybrid32Strategy>();
			return { true, "" };
		case ETileEntryFormat::Hybrid_64:
			m_strategy.emplace<THybrid64Strategy>();
			return { true, "" };
		default:
			return { false, "Unknown or unsupported map directory format" };
		}
	}

	void CMapFileWriter::InitProgressTasks(const Data::SLevelContext& context, Utils::Common::CProgressor* pProgressor)
	{
		if (!pProgressor)
			return;

		const size_t bytesPerTile = static_cast<size_t>(context.tileSize) * context.tileSize * m_bakeContext.channelsCount;
		const size_t tilesTotalBytes = m_bakeContext.nonEmptyTilesCount * bytesPerTile;

		size_t dirTotalBytes = m_bakeContext.bitmask.size() * sizeof(uint64);

		switch (m_bakeContext.entryFormat)
		{
		case ETileEntryFormat::Hybrid_32:
			dirTotalBytes += static_cast<size_t>(m_bakeContext.nonEmptyTilesCount + 1) * sizeof(uint32);
			break;
		case ETileEntryFormat::Hybrid_64:
			dirTotalBytes += static_cast<size_t>(m_bakeContext.nonEmptyTilesCount + 1) * sizeof(uint64);
			break;
		}

		m_bakeContext.pTilesTask = pProgressor->RegisterProgressTask(tilesTotalBytes, 1);
		m_bakeContext.pDirectoryTask = pProgressor->RegisterProgressTask(dirTotalBytes, 1);
	}
}