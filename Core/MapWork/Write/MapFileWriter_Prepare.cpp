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
	Data::SRunResult CMapFileWriter::BuildWriteContext(const SBakeContext& context, FileSystem::CPathResolver& pathResolver)
	{
		m_writeContext.channelsCount = context.baker.GetChannelCount();
		m_writeContext.layersMask = context.baker.GetActiveLayersMask();

		if (auto result = ValidateBakeContext(context); !result.bSuccess)
			return result;

		InitLayout(context);

		if (auto result = ComputeTilesOccupancy(context); !result.bSuccess)
			return result;


		if (auto result = ResolveMapPath(context, pathResolver); !result.bSuccess)
			return result;

		return { true, "" };
	}

	Data::SRunResult CMapFileWriter::ValidateBakeContext(const SBakeContext& context) const noexcept
	{
		if (context.levelContext.gridWidth <= 0 || context.levelContext.gridHeight <= 0)
			return { false, "Cannot bake: Level grid dimensions are invalid (Zero terrain size?)" };
		if (context.levelContext.tileSize == 0)
			return { false, "Cannot bake: Tile size must be greater than zero" };
		if (m_writeContext.channelsCount == 0)
			return { false, "Cannot bake: Baker returned zero channels" };
		return { true, "" };
	}

	void CMapFileWriter::InitLayout(const SBakeContext& context) noexcept
	{
		m_writeContext.mapType = context.baker.GetMapType();
		m_writeContext.tilesOffset = sizeof(SMapHeader);

		m_writeContext.tileCountX = (static_cast<uint32>(context.levelContext.gridWidth) + context.levelContext.tileSize - 1) / context.levelContext.tileSize;
		m_writeContext.tileCountY = (static_cast<uint32>(context.levelContext.gridHeight) + context.levelContext.tileSize - 1) / context.levelContext.tileSize;
		m_writeContext.totalTiles = static_cast<uint64>(m_writeContext.tileCountX) * m_writeContext.tileCountY;
	}

	Data::SRunResult CMapFileWriter::ComputeTilesOccupancy(const SBakeContext& context)
	{
		const size_t bitmaskSize = (m_writeContext.totalTiles + 63) / 64;
		if (!Utils::Common::TryAssign(m_writeContext.bitmask, bitmaskSize, 0))
			return { false, "Out of Memory: Failed to allocate tile occupancy data" };

		m_writeContext.nonEmptyTilesCount = 0;
		for (int32 y = 0; y < context.levelContext.gridHeight; ++y)
		{
			const uint32 ty = y / context.levelContext.tileSize;
			const size_t rowTileOffset = static_cast<size_t>(ty) * m_writeContext.tileCountX;
			const uint8* pRow = context.bakingData.data() + static_cast<size_t>(y) * context.levelContext.gridWidth * m_writeContext.channelsCount;

			uint tx = 0;
			uint32 xInTile = 0;

			for (int32 x = 0; x < context.levelContext.gridWidth; ++x)
			{
				const size_t tileIndex = rowTileOffset + tx;
				const size_t blockIndex = tileIndex / 64;
				const uint64 bitFlag = 1ULL << (tileIndex % 64);

				if ((m_writeContext.bitmask[blockIndex] & bitFlag) == 0 && PixelHasData(pRow, m_writeContext.channelsCount))
				{
					m_writeContext.bitmask[blockIndex] |= bitFlag;
					m_writeContext.nonEmptyTilesCount++;
				}

				pRow += m_writeContext.channelsCount;
				if (++xInTile == context.levelContext.tileSize)
					{ tx++; xInTile = 0; }
			}
		}
		return { true, "" };
	}

	Data::SRunResult CMapFileWriter::ResolveMapPath(const SBakeContext& context, FileSystem::CPathResolver& pathResolver)
	{
		if (auto resultPath = pathResolver.GetMapPath(context.baker.GetID()))
			m_writeContext.mapPath = std::move(*resultPath);
		else
			return { false, "Disk I/O Error: Cannot get map's path" };
		return { true, "" };
	}

	Data::SRunResult CMapFileWriter::InitCompressStrategy(const SBakeContext& context) noexcept
	{
		switch (context.compressAlg)
		{
		case ECompressionAlg::None:
			m_compressStrategy.emplace<std::monostate>();
			return { true, "" };
		case ECompressionAlg::Zlib:
			m_compressStrategy.emplace<TZlibCompressStrategy>();
			return { true, "" };
		case ECompressionAlg::Zstd:
			m_compressStrategy.emplace<TZstdCompressStrategy>();
			return { true, "" };
		case ECompressionAlg::LZ4:
			m_compressStrategy.emplace<TLZ4CompressStrategy>();
			return { true, "" };
		default:
			return { false, "Unknown or unsupported compression algorithm" };
		}
	}

	void CMapFileWriter::InitProgressTasks(const SBakeContext& context, Utils::Common::CProgressor* pProgressor)
	{
		if (!pProgressor)
			return;

		const size_t bytesPerTile = static_cast<size_t>(context.levelContext.tileSize) * context.levelContext.tileSize * m_writeContext.channelsCount;
		const size_t tilesTotalBytes = m_writeContext.nonEmptyTilesCount * bytesPerTile;

		size_t dirTotalBytes = m_writeContext.bitmask.size() * sizeof(uint64);

		switch (context.entryFormat)
		{
		case ETileEntryFormat::Hybrid_32:
			dirTotalBytes += static_cast<size_t>(m_writeContext.nonEmptyTilesCount + 1) * sizeof(uint32);
			break;
		case ETileEntryFormat::Hybrid_64:
			dirTotalBytes += static_cast<size_t>(m_writeContext.nonEmptyTilesCount + 1) * sizeof(uint64);
			break;
		}

		m_writeContext.pTilesTask = pProgressor->RegisterProgressTask(tilesTotalBytes);
		m_writeContext.pDirectoryTask = pProgressor->RegisterProgressTask(dirTotalBytes);

		const bool bCompressDir = (context.compressBlocks & static_cast<uint8>(ECompressedBlocks::Directory)) != 0;
		if (bCompressDir)
		{
			m_writeContext.pDirectoryFlushTask = pProgressor->ReserveProgressTask(10);
		}
	}
}