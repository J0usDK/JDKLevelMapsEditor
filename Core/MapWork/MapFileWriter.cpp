#include "StdAfx.h"
#include "MapFileWriter.h"

#include <utility>
#include <future>
#include <CrySystem/File/ICryPak.h>

#include "Core/Data/LevelContext.h"
#include "Core/Data/RunResult.h"
#include "Core/Bakers/IMapBaker.h"
#include "Core/ImageWork/Export/ImageExporter.h"
#include "Core/FileSystem/PathResolver.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/Progress.h"
#include "Utils/ScopedCryFile.h"

namespace
{
	inline bool PixelHasData(const uint8* pPixelData, uint32 numChannels) noexcept
	{
		for (uint32 c = 0; c < numChannels; ++c)
			if (pPixelData[c] != 0) return true;
		return false;
	}

	void ExtractTileDataHelper(
		const std::vector<uint8>& flatData,
		uint32 tileX, uint32 tileY,
		const JDKLevelMaps::Data::SLevelContext& context,
		uint32 numChannels,
		std::vector<uint8>& outTileBuffer) noexcept
	{
		uint32 maxLx = std::min(context.tileSize, static_cast<uint32>(context.gridWidth) - (tileX * context.tileSize));
		uint32 maxLy = std::min(context.tileSize, static_cast<uint32>(context.gridHeight) - (tileY * context.tileSize));

		if (maxLx < context.tileSize || maxLy < context.tileSize)
			std::memset(outTileBuffer.data(), 0, outTileBuffer.size());

		const size_t bytesPerRow = static_cast<size_t>(maxLx) * numChannels;
		const size_t localStride = static_cast<size_t>(context.tileSize) * numChannels;
		const size_t globalStride = static_cast<size_t>(context.gridWidth) * numChannels;

		size_t globalStart = ((static_cast<size_t>(tileY) * context.tileSize * context.gridWidth) + (tileX * context.tileSize)) * numChannels;
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
	Data::SRunResult CMapFileWriter::Prepare(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor, BakeData bakingData)
	{
		Data::SRunResult result;

		result = BuildBakeContext(baker, pathResolver, context, bakingData);
		if (!result.bSuccess)
			return result;

		if (pProgressor)
		{
			size_t tilesOpWeight = context.tileSize * context.tileSize * m_bakeContext.numChannels;
			size_t dirOpWeight = m_bakeContext.bUseCompact ? sizeof(STileEntry32) : sizeof(STileEntry64);

			m_bakeContext.pTilesTask = pProgressor->RegisterProgressTask(m_bakeContext.nonEmptyTilesCount, tilesOpWeight);
			m_bakeContext.pDirectoryTask = pProgressor->RegisterProgressTask(m_bakeContext.totalTiles, dirOpWeight);
		}

		m_bakeContext.bReady = true;
		return { true, "" };
	}

	Data::SRunResult CMapFileWriter::BakeMap(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, BakeData bakingData)
	{
		if (!std::exchange(m_bakeContext.bReady, false))
			return { false, "Map Writer is not ready" };

		if (m_bakeContext.bUseCompact)
			return WriteMap<STileEntry32>(context, bakingData);
		else
			return WriteMap<STileEntry64>(context, bakingData);
	}

	Data::SRunResult CMapFileWriter::BuildBakeContext(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, const Data::SLevelContext& context, BakeData bakingData)
	{
		if (context.gridWidth <= 0 || context.gridHeight <= 0)
			return { false, "Cannot bake: Level grid dimensions are invalid (Zero terrain size?)" };

		m_bakeContext.numChannels = baker.GetChannelCount();

		if (context.tileSize == 0)
			return { false, "Cannot bake: Tile size must be greater than zero" };
		if (m_bakeContext.numChannels == 0)
			return { false, "Cannot bake: Baker returned zero channels" };

		m_bakeContext.tileCountX = (static_cast<uint32>(context.gridWidth) + context.tileSize - 1) / context.tileSize;
		m_bakeContext.tileCountY = (static_cast<uint32>(context.gridHeight) + context.tileSize - 1) / context.tileSize;
		m_bakeContext.totalTiles = static_cast<uint64>(m_bakeContext.tileCountX) * m_bakeContext.tileCountY;

		auto result = ComputeTilesOccupancy(context, bakingData);
		if (!result.bSuccess)
			return result;

		const uint64 fullTileByteSize = static_cast<uint64>(context.tileSize) * context.tileSize * m_bakeContext.numChannels;
		const uint64 actualSize32 = sizeof(SMapHeader) + (m_bakeContext.totalTiles * sizeof(STileEntry32)) + (m_bakeContext.nonEmptyTilesCount * fullTileByteSize);
		
		m_bakeContext.directoryOffset = sizeof(SMapHeader);
		m_bakeContext.bUseCompact = (actualSize32 <= UINT32_MAX);
		m_bakeContext.tilesOffset = sizeof(SMapHeader) + (m_bakeContext.totalTiles * (m_bakeContext.bUseCompact ? sizeof(STileEntry32) : sizeof(STileEntry64)));
		m_bakeContext.mapType = baker.GetMapType();

		if (auto resultPath = pathResolver.GetMapPath(baker.GetID()))
			m_bakeContext.mapPath = resultPath.value();
		else
			return { false, "Disk I/O Error: Cannot get map's path" };

		return { true, "" };
	}

	Data::SRunResult CMapFileWriter::ComputeTilesOccupancy(const Data::SLevelContext& context, BakeData bakingData)
	{
		try
		{
			m_bakeContext.tilesOccupancy.assign(m_bakeContext.totalTiles, 0);
		}
		catch (const std::bad_alloc&)
		{
			return { false, "Out of Memory: Failed to allocate tile occupancy data" };
		}

		for (int32 y = 0; y < context.gridHeight; ++y)
		{
			uint32 ty = y / context.tileSize;
			size_t rowTileOffset = static_cast<size_t>(ty) * m_bakeContext.tileCountX;
			size_t globalRowIndex = static_cast<size_t>(y) * context.gridWidth * m_bakeContext.numChannels;

			uint tx = 0;
			uint32 xInTile = 0;

			for (int32 x = 0; x < context.gridWidth; ++x)
			{
				size_t tileIndex = rowTileOffset + tx;

				if (!m_bakeContext.tilesOccupancy[tileIndex] && PixelHasData(bakingData.data() + globalRowIndex + (x * m_bakeContext.numChannels), m_bakeContext.numChannels))
				{
					m_bakeContext.tilesOccupancy[tileIndex] = 1;
					m_bakeContext.nonEmptyTilesCount++;
				}

				if (++xInTile == context.tileSize)
				{
					tx++;
					xInTile = 0;
				}
			}
		}
		return { true, "" };
	}

	template<typename TTileEntry>
	Data::SRunResult CMapFileWriter::WriteMap(const Data::SLevelContext& context, BakeData bakingData)
	{
		Data::SRunResult result;

		std::vector<TTileEntry> directory;
		try
		{
			directory.resize(m_bakeContext.totalTiles);
		}
		catch (const std::bad_alloc&)
		{
			return { false, "Out of Memory: Failed to allocate directory metadata" };
		}

		Utils::FileSystem::ScopedCryFile pFile(FileSystem::LFSFacade::FOpen(m_bakeContext.mapPath.c_str(), "wb", true), m_bakeContext.mapPath.c_str());
		if (!pFile)
			return { false, "Disk I/O Error: Cannot open map file for writing" };

		if (!WriteHeader(context, pFile))
			return { false, "Disk I/O Error: Cannot write map's header" };

		result = WriteTiles(context, pFile, bakingData, directory);
		if (!result.bSuccess)
			return result;

		result = WriteDirectory(pFile, directory);
		if (!result.bSuccess)
			return result;

		pFile.close(true);
		return { true, "Map was written to: " + m_bakeContext.mapPath };
	}

	bool CMapFileWriter::WriteHeader(const Data::SLevelContext& context, Utils::FileSystem::ScopedCryFile& file)
	{
		SMapHeader header;
		header.mapType = m_bakeContext.mapType;
		header.entryFormat = m_bakeContext.bUseCompact ? ETileEntryFormat::Compact_32 : ETileEntryFormat::Standard_64;
		header.gridWidth = context.gridWidth;
		header.gridHeight = context.gridHeight;
		header.cellSize = context.cellSize;
		header.originX = context.originX;
		header.originY = context.originY;
		header.tileSize = context.tileSize;
		header.tileCountX = m_bakeContext.tileCountX;
		header.tileCountY = m_bakeContext.tileCountY;

		return gEnv->pCryPak->FWrite(&header, sizeof(JDKLevelMaps::SMapHeader), 1, file) == 1;
	}

	template<typename TTileEntry>
	Data::SRunResult CMapFileWriter::WriteTiles(const Data::SLevelContext& context, Utils::FileSystem::ScopedCryFile& file, BakeData bakingData, std::vector<TTileEntry>& directory)
	{
		Data::SRunResult result;

		std::vector<uint8> tileBuffer;
		try
		{
			tileBuffer.resize(static_cast<size_t>(context.tileSize) * context.tileSize * m_bakeContext.numChannels);
		}
		catch (std::bad_alloc&)
		{
			return { false, "Out of Memory: Failed to allocate memory for tile buffer during file writing" };
		}

		uint32 tx = 0;
		uint32 ty = 0;
		uint64 nonEmptyProcessed = 0;

		JDKLevelMaps::FileSystem::LFSFacade::FSeek(file, m_bakeContext.tilesOffset, SEEK_SET);

		for (size_t tileIndex = 0; tileIndex < m_bakeContext.totalTiles; ++tileIndex)
		{
			result = WriteTile(context, file, tileBuffer, directory, bakingData, tileIndex, tx, ty, nonEmptyProcessed);
			if (!result.bSuccess)
				return result;

			if (++tx == m_bakeContext.tileCountX)
			{
				tx = 0;
				++ty;
			}
		}

		return { true, "" };
	}

	template<typename TTileEntry>
	Data::SRunResult CMapFileWriter::WriteTile(
		const Data::SLevelContext& context,
		Utils::FileSystem::ScopedCryFile& file,
		std::vector<uint8>& tileBuffer,
		std::vector<TTileEntry>& directory,
		BakeData bakingData,
		size_t tileIndex,
		uint32 tx, uint32 ty,
		uint64& nonEmptyProcessed)
	{
		if (!m_bakeContext.tilesOccupancy[tileIndex])
		{
			directory[tileIndex].fileOffset = 0;
			directory[tileIndex].byteSize = 0;
			return { true, "" };
		}

		ExtractTileDataHelper(bakingData, tx, ty, context, m_bakeContext.numChannels, tileBuffer);
		uint64 currentOffset = JDKLevelMaps::FileSystem::LFSFacade::FTell(file);

		directory[tileIndex].fileOffset = static_cast<decltype(TTileEntry::fileOffset)>(currentOffset);
		directory[tileIndex].byteSize = static_cast<decltype(TTileEntry::byteSize)>(tileBuffer.size());

		const double invBufferSize = 1.0 / static_cast<double>(tileBuffer.size());
		if (!WriteDataChunked(file, tileBuffer.data(), 1, tileBuffer.size(),
			[&](size_t written)
			{
				if (!m_bakeContext.pTilesTask) return true;

				double fraction = static_cast<double>(written) * invBufferSize;
				return m_bakeContext.pTilesTask->Update(static_cast<double>(nonEmptyProcessed) + fraction);
			}))
		{
			return { false, "Disk I/O Error: Failed to write tile data at index: " + std::to_string(tileIndex) };
		}

		nonEmptyProcessed++;
		return { true, "" };
	}

	template<typename TTileEntry>
	Data::SRunResult CMapFileWriter::WriteDirectory(Utils::FileSystem::ScopedCryFile& file, std::vector<TTileEntry>& directory)
	{
		JDKLevelMaps::FileSystem::LFSFacade::FSeek(file, m_bakeContext.directoryOffset, SEEK_SET);

		if (!WriteDataChunked(file, directory.data(), sizeof(TTileEntry), directory.size(),
			[&](size_t written) { return m_bakeContext.pDirectoryTask ? m_bakeContext.pDirectoryTask->Update(written) : true; }))
		{
			return { false, "Disk I/O Error: Cannot finalize directory metadata" };
		}

		return { true, "" };
	}

	template<typename TProgressCallback>
	bool CMapFileWriter::WriteDataChunked(Utils::FileSystem::ScopedCryFile& file, const void* pData, size_t elementSize, size_t totalElements, TProgressCallback progressCallback)
	{
		if (totalElements == 0)
			return true;

		const size_t elementPerChunk = std::max<size_t>(1, kMaxChunkSize / elementSize);
		size_t elementsWritten = 0;
		const uint8* pByteData = static_cast<const uint8*>(pData);

		while (elementsWritten < totalElements)
		{
			size_t elementsToWrite = std::min(elementPerChunk, totalElements - elementsWritten);
			if (gEnv->pCryPak->FWrite(pByteData + (elementsWritten * elementSize), elementSize, elementsToWrite, file) != elementsToWrite)
				return false;
			elementsWritten += elementsToWrite;

			if (!progressCallback(elementsWritten))
				return false;
		}
		return true;
	}
}