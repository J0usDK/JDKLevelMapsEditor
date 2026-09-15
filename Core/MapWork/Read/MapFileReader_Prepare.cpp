#include "StdAfx.h"
#include "MapFileReader.h"

#include <CrySystem/File/ICryPak.h>

#include "Core/Data/RunResult.h"
#include "Core/Bakers/IMapBaker.h"
#include "Core/FileSystem/PathResolver.h"

namespace JDKLevelMaps::MapWork
{
	Data::SRunResult CMapFileReader::BuildContext(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver)
	{
		if (auto mapPath = pathResolver.GetMapPath(baker.GetID()))
			m_readContext.mapPath = std::move(*mapPath);
		else
			return { false, "Disk I/O Error: Cannot get map's path" };

		m_readContext.pColorMapper = baker.GetDebugColorMapper();
		return { true, "" };
	}

	Data::SRunResult CMapFileReader::ReadMapHeader(FILE* pFile)
	{
		if (gEnv->pCryPak->FReadRaw(&m_readContext.header, sizeof(SMapHeader), 1, pFile) != 1)
			return { false, "Disk I/O Error: Cannot read map's header" };

		if (!IsValidMapHeader(m_readContext.header))
			return { false, "Invalid map file (file is corrupted?)" };

		m_readContext.channelsCount = static_cast<uint32>(std::bitset<8>(m_readContext.header.activeLayersMask).count());
		if (m_readContext.channelsCount == 0)
			return { false, "Invalid map file: No active layers found in header" };

		return { true, "" };
	}

	Data::SRunResult CMapFileReader::InitFormatStrategy() noexcept
	{
		switch (m_readContext.header.entryFormat)
		{
		case ETileEntryFormat::Bitmask:
			m_formatStrategy.emplace<TBitmaskStrategy>();
			return { true, "" };
		case ETileEntryFormat::Hybrid_32:
			m_formatStrategy.emplace<THybrid32Strategy>();
			return { true, "" };
		case ETileEntryFormat::Hybrid_64:
			m_formatStrategy.emplace<THybrid64Strategy>();
			return { true, "" };
		default:
			return { false, "Unknown or unsupported map directory format" };
		}
	}

	Data::SRunResult CMapFileReader::InitCompressStrategy() noexcept
	{
		switch (m_readContext.header.compressionAlg)
		{
		case ECompressionAlg::None:
			m_decompressStrategy.emplace<std::monostate>();
			return { true, "" };
		case ECompressionAlg::Zlib:
			m_decompressStrategy.emplace<TZlibCompressStrategy>();
			return { true, "" };
		case ECompressionAlg::Zstd:
			m_decompressStrategy.emplace<TZstdCompressStrategy>();
			return { true, "" };
		case ECompressionAlg::LZ4:
			m_decompressStrategy.emplace<TLZ4CompressStrategy>();
			return { true, "" };
		default:
			return { false, "Unknown or unsupported compression algorithm" };
		}
	}

	void CMapFileReader::InitProgressTasks(Utils::Common::CProgressor* pProgressor)
	{
		if (!pProgressor)
			return;

		const bool bCompressDir = (m_readContext.header.compressedBlocks & static_cast<uint8>(ECompressedBlocks::Directory)) != 0;
		const size_t dirBytesToRead = bCompressDir ? m_readContext.header.compressedDirectorySize : m_readContext.header.rawDirectorySize;

		const uint64 totalTiles = static_cast<uint64>(m_readContext.header.tileCountX) * m_readContext.header.tileCountY;

		m_readContext.pDirTask = pProgressor->RegisterProgressTask(dirBytesToRead);
		m_readContext.pTilesTask = pProgressor->RegisterProgressTask(totalTiles);
	}

	const Strategies::ICompressionStrategy* CMapFileReader::GetDecompressor() const noexcept
	{
		return std::visit([](auto&& strategy) -> const Strategies::ICompressionStrategy* {
			using T = std::decay_t<decltype(strategy)>;
			if constexpr (std::is_same_v<T, std::monostate>)
				return nullptr;
			else return &strategy;
		}, m_decompressStrategy);
	}

	Strategies::IDirectoryFormatStrategy* CMapFileReader::GetFormat() noexcept
	{
		return std::visit([](auto&& strategy) -> Strategies::IDirectoryFormatStrategy* {
			using T = std::decay_t<decltype(strategy)>;
			if constexpr (std::is_same_v<T, std::monostate>)
				return nullptr;
			else return &strategy;
		}, m_formatStrategy);
	}
}