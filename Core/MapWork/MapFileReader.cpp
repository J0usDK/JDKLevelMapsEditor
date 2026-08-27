#include "StdAfx.h"
#include "MapFileReader.h"

#include <QImage>
#include <CrySystem/File/ICryPak.h>

#include "Core/ImageWork/MapImageConverter.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Core/FileSystem/PathResolver.h"
#include "Core/Data/RunResult.h"
#include "Utils/Logger.h"
#include "Utils/Progress.h"
#include "Utils/ScopedCryFile.h"

namespace JDKLevelMaps::MapWork
{
	Data::SRunResult CMapFileReader::Prepare(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor)
	{
		Data::SRunResult result;

		result = BuildContext(baker, pathResolver);
		if (!result.bSuccess)
			return result;

		Utils::FileSystem::ScopedCryFile file(FileSystem::LFSFacade::FOpen(m_readContext.mapPath.c_str(), "rb"));
		if (!file)
			return { false, "Disk I/O Error: Cannot open map file for reading" };

		result = ReadMapHeader(file);
		if (!result.bSuccess)
			return result;

		if (pProgressor)
		{
			const uint64 totalTiles = static_cast<uint64>(m_readContext.mapHeader.tileCountX) * m_readContext.mapHeader.tileCountY;
			m_readContext.pReadTask = pProgressor->RegisterProgressTask(totalTiles, 1);
		}

		m_readContext.bReady = true;
		return { true, "" };
	}

	Data::SRunResult CMapFileReader::LoadPreviewFromMap(ImageWork::SImageView& outImage)
	{
		if (!m_readContext.bReady)
			return { false, "Map Reader is not ready" };
		else m_readContext.bReady = false;

		Utils::FileSystem::ScopedCryFile file(FileSystem::LFSFacade::FOpen(m_readContext.mapPath.c_str(), "rb"));
		if (!file)
			return { false, "Disk I/O Error: Cannot open map file for reading" };

		outImage.Resize(m_readContext.mapHeader.gridWidth, m_readContext.mapHeader.gridHeight);
		if (!outImage.IsValid())
			return { false, "Out of Memory: Failed to allocate QImage" };

		Data::SRunResult result;

		if (m_readContext.mapHeader.entryFormat == ETileEntryFormat::Compact_32)
			result = ReadTiles<STileEntry32>(file, outImage);
		else
			result = ReadTiles<STileEntry64>(file, outImage);

		if (!result.bSuccess)
			return result;
		return { true, "" };
	}

	Data::SRunResult CMapFileReader::BuildContext(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver)
	{
		if (auto mapPath = pathResolver.GetMapPath(baker.GetID()))
			m_readContext.mapPath = mapPath.value();
		else
			return { false, "Disk I/O Error: Cannot get map's path" };

		m_readContext.channelsCount = baker.GetChannelCount();
		m_readContext.pColorMapper = baker.GetDebugColorMapper();
		return { true, "" };
	}

	Data::SRunResult CMapFileReader::ReadMapHeader(Utils::FileSystem::ScopedCryFile& file)
	{
		if (gEnv->pCryPak->FReadRaw(&m_readContext.mapHeader, sizeof(SMapHeader), 1, file) != 1)
			return { false, "Disk I/O Error: Cannot read map's header" };

		if (!IsValidMapHeader(m_readContext.mapHeader))
			return { false, "Invalid map file (file is corrupted?)" };

		return { true, "" };
	}

	template<typename TTileEntry>
	[[nodiscard]] Data::SRunResult CMapFileReader::ReadTiles(Utils::FileSystem::ScopedCryFile& dirFile, ImageWork::SImageView& outImage)
	{
		Data::SRunResult result;
		ImageWork::Converters::SConvertContext convertCtx(m_readContext.mapHeader, m_readContext.channelsCount, outImage, m_readContext.pColorMapper);

		const uint64 totalTiles = static_cast<uint64>(m_readContext.mapHeader.tileCountX) * m_readContext.mapHeader.tileCountY;
		std::vector<TTileEntry> dirChunk(kDirectoryChunkSize);

		std::vector<uint8> tileBuffer;
		result = PrepareTileBuffer(tileBuffer);
		if (!result.bSuccess)
			return result;

		if (FileSystem::LFSFacade::FSeek(dirFile, sizeof(SMapHeader), SEEK_SET) != 0)
			return { false, "Disk I/O Error: Cannot seek to map directory (file is corrupted?)" };

		JDKLevelMaps::Utils::FileSystem::ScopedCryFile tileFile(FileSystem::LFSFacade::FOpen(m_readContext.mapPath.c_str(), "rb"));
		if (!tileFile)
			return { false, "Disk I/O Error: Can't open map file" };

		uint64 tilesRead = 0;
		while (tilesRead < totalTiles)
		{
			const uint64 remainingTiles = totalTiles - tilesRead;
			const size_t toRead = static_cast<size_t>(std::min<uint64>(kDirectoryChunkSize, remainingTiles));

			result = ReadTileChunk(dirFile, tileFile, dirChunk, tileBuffer, tilesRead, toRead, convertCtx);
			if (!result.bSuccess)
				return result;

			tilesRead += toRead;

			if (m_readContext.pReadTask && !m_readContext.pReadTask->Update(tilesRead))
				return { false, "Preview loading was cancelled by user" };
		}
		return { true, "" };
	}

	inline Data::SRunResult CMapFileReader::PrepareTileBuffer(std::vector<uint8>& tileBuffer) const
	{
		const size_t maxTileSize = static_cast<size_t>(m_readContext.mapHeader.tileSize) * m_readContext.mapHeader.tileSize * m_readContext.channelsCount;

		try
		{
			tileBuffer.resize(maxTileSize);
		}
		catch (const std::bad_alloc&)
		{
			return { false, "Out of Memory: Failed to allocate memory for tile buffer during map reading" };
		}

		return { true, "" };
	}

	template<typename TTileEntry>
	inline Data::SRunResult CMapFileReader::ReadTileChunk(FILE* dirFile, FILE* tileFile, std::vector<TTileEntry>& dirChunk, std::vector<uint8>& tileBuffer, uint64 tilesRead, size_t tileCount, ImageWork::Converters::SConvertContext& convertCtx)
	{
		if (gEnv->pCryPak->FReadRaw(dirChunk.data(), sizeof(TTileEntry), tileCount, dirFile) != tileCount)
			return { false, "Disk I/O Error: Cannot read map's directory (file is corrupted?)" };

		for (size_t i = 0; i < tileCount; ++i)
			if (auto result = ReadTile(dirChunk[i], tilesRead + i, tileFile, tileBuffer, convertCtx); !result.bSuccess)
				return result;

		return { true, "" };
	}

	template<typename TTileEntry>
	inline Data::SRunResult CMapFileReader::ReadTile(const TTileEntry& entry, uint64 tileIndex, FILE* pFile, std::vector<uint8>& tileBuffer, ImageWork::Converters::SConvertContext& convertCtx)
	{
		if (entry.byteSize == 0)
			return { true, "" };
		if (entry.byteSize > tileBuffer.size())
			return { false, "Invalid tile size (file is corrupted?)" };

		if (FileSystem::LFSFacade::FSeek(pFile, entry.fileOffset, SEEK_SET) != 0)
			return { false, "Disk I/O Error: Cannot seek to map tile (file is corrupted?)" };
		if (gEnv->pCryPak->FReadRaw(tileBuffer.data(), 1, entry.byteSize, pFile) != entry.byteSize)
			return { false, "Disk I/O Error: Cannot read map's tiles (file is corrupted?)" };

		const uint32 tx = static_cast<uint32>(tileIndex % m_readContext.mapHeader.tileCountX);
		const uint32 ty = static_cast<uint32>(tileIndex / m_readContext.mapHeader.tileCountX);
		ImageWork::Converters::MapTileToImage(tileBuffer, tx, ty, convertCtx);

		return { true, "" };
	}
}