#include "StdAfx.h"
#include "MapsBaker.h"

#include <future>

#include "Core/Data/LevelContext.h"
#include "Core/Data/RunResult.h"
#include "Core/BakersRegistry.h"
#include "Core/Bakers/IMapBaker.h"
#include "Core/Bakers/Vegetation/EditorVegetationSource.h"
#include "Core/FileSystem/PathResolver.h"
#include "Core/MapWork/Write/MapFileWriter.h"
#include "Core/ImageWork/Export/ImageExporter.h"
#include "Settings/BakerSettings.h"
#include "Utils/Progress.h"

namespace JDKLevelMaps::Managers
{
	static const constexpr uint64 kMinTileSizeToCompress = 256; // 256 BYTES
	static const constexpr uint64 kMinDataSizeToCompress = 5 * 1024 * 1024; // 5 MB

	struct SMapFormats
	{
		ETileEntryFormat entryFormat = ETileEntryFormat::Bitmask;
		ECompressionAlg compressiongAlg = ECompressionAlg::None;
		uint8 compressBlocks = 0;
	};

	static uint64 ComputeDataSize(uint64 nonEmptyTilesCount, uint64 tileSize, uint32 channelsCount) noexcept
	{
		const uint64 rawTileSize = static_cast<uint64>(tileSize) * tileSize * channelsCount;
		return nonEmptyTilesCount * rawTileSize;
	}

	static uint64 ComputeBitmaskSize(int32 gridWidth, int32 gridHeight, uint32 tileSize) noexcept
	{
		const uint32 tileCountX = (static_cast<uint32>(gridWidth) + tileSize - 1) / tileSize;
		const uint32 tileCountY = (static_cast<uint32>(gridHeight) + tileSize - 1) / tileSize;
		return static_cast<uint64>(tileCountX) * tileCountY;
	}

	static uint64 ComputeOffsetsSize(uint64 nonEmptyTilesCount)
	{
		return nonEmptyTilesCount * sizeof(uint32);
	}

	static uint64 ComputeFullSize(uint64 dataSize, uint64 bitmaskSize, uint64 offsetsSize) noexcept
	{
		return sizeof(SMapHeader) + dataSize + bitmaskSize + offsetsSize;
	}
}

namespace JDKLevelMaps::Managers
{
	CMapsBaker::CMapsBaker(CBakersRegistry& bakersRegistry, FileSystem::CPathResolver& pathResolver, const Settings::SBakerSettings& bakerSettings) noexcept
		: m_bakersRegistry(bakersRegistry), m_pathResolver(pathResolver), m_bakerSettings(bakerSettings) { }

	Data::SRunResult CMapsBaker::RunBake(EMapType bakerType, Utils::Common::SProgress& progress)
	{
		const auto pBaker = m_bakersRegistry.GetBaker(bakerType);
		if (!pBaker)
			return { false, "Cannot get baker" };

		Data::SLevelContext context = Data::ComputeLevelContext(m_bakerSettings.cellSize, m_bakerSettings.tileSize);
		std::vector<uint8> bakingData = pBaker->Bake(context);

		MapWork::CMapFileWriter mapFileWriter;
		ImageWork::CImageExporter imageExporter;
		Utils::Common::CProgressor progressor(progress);

		const ECompressionAlg compressAlg = ResolveCompressionAlg(m_bakerSettings.compression);
		MapWork::SBakeContext bakeContext(*pBaker, context, bakingData, compressAlg);

		if (auto result = mapFileWriter.Prepare(bakeContext, m_pathResolver); !result.bSuccess)
			return result;

		const uint64 nonEmptyTilesCount = mapFileWriter.GetNonEmptyTilesCount();
		bakeContext.compressBlocks = ResolveCompressBlocks(*pBaker, m_bakerSettings.compression, context.tileSize, nonEmptyTilesCount);
		bakeContext.entryFormat = ResolveEntryFormat(*pBaker, context, m_bakerSettings.directoryFormat, nonEmptyTilesCount, bakeContext.compressBlocks);

		if (m_bakerSettings.bGenerateDebugImage)
			if (auto result = imageExporter.Prepare(*pBaker, context, m_pathResolver, &progressor); !result.bSuccess)
				return result;

		std::future<Data::SRunResult> mapWriteTask = std::async(std::launch::async, [&]()
			{ return mapFileWriter.BakeMap(bakeContext, &progressor); });

		std::future<Data::SRunResult> imageExportTask = std::async(std::launch::async, [&]()
			{ return m_bakerSettings.bGenerateDebugImage ? imageExporter.ExportImage(*pBaker, context, bakingData) : Data::SRunResult(true, ""); });

		Data::SRunResult mapResult = mapWriteTask.get();
		Data::SRunResult imageResult = imageExportTask.get();

		if (mapResult.bSuccess && imageResult.bSuccess)
			return { true, "" };
		if (!mapResult.bSuccess)
			return mapResult;

		return imageResult;
	}

	ECompressionAlg CMapsBaker::ResolveCompressionAlg(Settings::ECompression compression) noexcept
	{
		switch (compression)
		{
		case Settings::ECompression::None: return ECompressionAlg::None;
		case Settings::ECompression::Auto: return ECompressionAlg::Zstd;
		case Settings::ECompression::ZlibMetadata: return ECompressionAlg::Zlib;
		case Settings::ECompression::ZlibData: return ECompressionAlg::Zlib;
		case Settings::ECompression::ZlibBoth: return ECompressionAlg::Zlib;
		case Settings::ECompression::ZstdMetadata: return ECompressionAlg::Zstd;
		case Settings::ECompression::ZstdData: return ECompressionAlg::Zstd;
		case Settings::ECompression::ZstdBoth: return ECompressionAlg::Zstd;
		case Settings::ECompression::LZ4Metadata: return ECompressionAlg::LZ4;
		case Settings::ECompression::LZ4Data: return ECompressionAlg::LZ4;
		case Settings::ECompression::LZ4Both: return ECompressionAlg::LZ4;
		default: return ECompressionAlg::None;
		}
	}

	uint8 CMapsBaker::ResolveCompressBlocks(const Bakers::IMapBaker& baker, Settings::ECompression comp, uint32 tileSize, uint64 nonEmptyTilesCount) noexcept
	{
		if (comp == Settings::ECompression::None)
			return static_cast<uint8>(ECompressedBlocks::None);
		if (comp == Settings::ECompression::ZlibMetadata || comp == Settings::ECompression::ZstdMetadata || comp == Settings::ECompression::LZ4Metadata)
			return static_cast<uint8>(ECompressedBlocks::Directory);
		if (comp == Settings::ECompression::ZlibData || comp == Settings::ECompression::ZstdData || comp == Settings::ECompression::LZ4Data)
			return static_cast<uint8>(ECompressedBlocks::Tiles);
		if (comp == Settings::ECompression::ZlibBoth || comp == Settings::ECompression::ZstdBoth || comp == Settings::ECompression::LZ4Both)
			return static_cast<uint8>(ECompressedBlocks::Directory | ECompressedBlocks::Tiles);

		// Auto
		const uint32 channelsCount = baker.GetChannelCount();
		const uint64 rawTileSize = static_cast<uint64>(tileSize) * tileSize * channelsCount;
		const uint64 dataSize = nonEmptyTilesCount * rawTileSize;

		if (rawTileSize >= kMinTileSizeToCompress && dataSize >= kMinDataSizeToCompress)
			return static_cast<uint8>(ECompressedBlocks::Directory | ECompressedBlocks::Tiles);
		else
			return static_cast<uint8>(ECompressedBlocks::Directory);
	}

	ETileEntryFormat CMapsBaker::ResolveEntryFormat(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, Settings::EDirectoryFormat dirFormat, uint64 nonEmptyTilesCount, uint8 compBlocksMask) noexcept
	{
		if (dirFormat == Settings::EDirectoryFormat::Bitmask)
			return ETileEntryFormat::Bitmask;
		if (dirFormat != Settings::EDirectoryFormat::Hybrid)
			if ((compBlocksMask & static_cast<uint8>(ECompressedBlocks::Tiles)) == 0)
				return ETileEntryFormat::Bitmask;

		const uint64 dataSize = ComputeDataSize(nonEmptyTilesCount, context.tileSize, baker.GetChannelCount());
		const uint64 bitmaskSize = ComputeBitmaskSize(context.gridWidth, context.gridHeight, context.tileSize);
		const uint64 offsetsSize = ComputeOffsetsSize(nonEmptyTilesCount);
		const uint64 mapSize = ComputeFullSize(dataSize, bitmaskSize, offsetsSize);

		return mapSize <= UINT32_MAX ? ETileEntryFormat::Hybrid_32 : ETileEntryFormat::Hybrid_64;
	}
}