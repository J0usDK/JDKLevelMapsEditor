#include "StdAfx.h"
#include "MapFileReader.h"

#include "Core/Data/RunResult.h"
#include "Core/ImageWork/MapImageConverter.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/VectorUtils.h"
#include "Utils/Progress.h"

namespace JDKLevelMaps::MapWork
{
	struct SDecompressor final
	{
		const Strategies::ICompressionStrategy* pStrategy = nullptr;
		std::unique_ptr<Strategies::IDecompressionContext> pContext = nullptr;

		std::vector<uint8> decompressedBuffer;

		explicit SDecompressor(const Strategies::ICompressionStrategy* pStrategy)
			: pStrategy(pStrategy) { }

		[[nodiscard]] Data::SRunResult Begin() noexcept
		{
			if (!pStrategy)
				return { false, "Internal Error: Compressor not initialized" };

			decompressedBuffer.clear();

			pContext = pStrategy->BeginDecompression();
			return pContext ? Data::SRunResult{ true, "" } : Data::SRunResult{ false, "Out of Memory: Failed to init decompression context" };
		}

		template<typename TCallback>
		[[nodiscard]] Data::SRunResult ProcessChunked(FILE* pFile, size_t totalBytesToRead, TCallback callback)
		{
			static const constexpr size_t CHUNK_SIZE = 1024 * 1024;

			std::vector<uint8> readBuf;
			if (!Utils::Common::TryResize(readBuf, CHUNK_SIZE))
				return { false, "Out of Memory: Failed to allocate chunk buffer" };

			size_t bytesRead = 0;
			while (bytesRead < totalBytesToRead)
			{
				const size_t toRead = std::min(CHUNK_SIZE, totalBytesToRead - bytesRead);
				if (gEnv->pCryPak->FReadRaw(readBuf.data(), 1, toRead, pFile) != toRead)
					return { false, "Disk I/O Error: Cannot read compressed chunk" };

				if (auto result = pStrategy->DecompressChunk(*pContext, readBuf.data(), toRead, decompressedBuffer); !result.bSuccess)
					return result;

				bytesRead += toRead;
				if (!callback(bytesRead))
					return { false, "Operation cancelled by user" };
			}
			
			return { true, "" };
		}
	};
}

namespace JDKLevelMaps::MapWork
{
	Data::SRunResult CMapFileReader::ReadMap(FILE* pFile, ImageWork::SImageView& outImage)
	{
		std::vector<uint8> rawDirectory;
		if (auto result = ReadDirectory(pFile, rawDirectory); !result.bSuccess)
			return result;

		if (auto result = ReadTiles(pFile, rawDirectory, outImage); !result.bSuccess)
			return result;

		return { true, "" };
	}

	Data::SRunResult CMapFileReader::ReadDirectory(FILE* pFile, std::vector<uint8>& outDirectory)
	{
		const bool bCompressDir = (m_readContext.header.compressedBlocks & static_cast<uint8>(ECompressedBlocks::Directory)) != 0;

		FileSystem::LFSFacade::FSeek(pFile, m_readContext.header.directoryOffset, SEEK_SET);

		auto dirProgress = [&](size_t bytesRead) -> bool {
			return m_readContext.pDirTask ? m_readContext.pDirTask->Update(static_cast<double>(bytesRead)) : true;
		};

		if (bCompressDir)
		{
			SDecompressor decompressor(GetDecompressor());
			if (auto result = decompressor.Begin(); !result.bSuccess)
				return result;

			if (auto result = decompressor.ProcessChunked(pFile, m_readContext.header.compressedDirectorySize, dirProgress); !result.bSuccess)
				return result;

			outDirectory = std::move(decompressor.decompressedBuffer);
		}
		else
		{
			if (!Utils::Common::TryResize(outDirectory, m_readContext.header.rawDirectorySize))
				return { false, "Out of Memory: Failed to allocate raw directory buffer" };

			if (auto result = Utils::FileSystem::ReadDataChunked(pFile, outDirectory.data(), 1, outDirectory.size(), dirProgress); !result.bSuccess)
				return result;
		}

		return { true, "" };
	}

	Data::SRunResult CMapFileReader::ReadTiles(FILE* pFile, const std::vector<uint8>& directory, ImageWork::SImageView& outImage)
	{
		ImageWork::Converters::SConvertContext convertCtx(m_readContext.header, m_readContext.channelsCount, m_readContext.header.activeLayersMask, outImage, m_readContext.pColorMapper);

		if (auto result = GetFormat()->ParseDirectory(directory, m_readContext); !result.bSuccess)
			return result;

		std::vector<uint8> tileBuffer;
		const Strategies::ICompressionStrategy* pDecompressor = GetDecompressor();
		const uint64 totalTiles = static_cast<uint64>(m_readContext.header.tileCountX) * m_readContext.header.tileCountY;
		const uint64 maxTileSize = static_cast<uint64>(m_readContext.header.tileSize) * m_readContext.header.tileSize * m_readContext.channelsCount;

		if (!Utils::Common::TryResize(tileBuffer, maxTileSize))
			return { false, "Out of Memory: Failed to allocate raw tile buffer" };

		SDecompressor decompressor(pDecompressor);

		for (uint64 i = 0; i < totalTiles; ++i)
			if (auto result = ReadTile(i, pFile, tileBuffer, decompressor, convertCtx); !result.bSuccess)
				return result;

		return { true, "" };

	}

	inline Data::SRunResult CMapFileReader::ReadTile(uint64 tileIndex, FILE* pFile, std::vector<uint8>& tileBuffer, SDecompressor& decompressor, ImageWork::Converters::SConvertContext& convertCtx)
	{
		const auto* pFormat = GetFormat();
		const auto tileNode = pFormat ? pFormat->GetTileNode(tileIndex) : std::nullopt;

		if (!tileNode.has_value() || tileNode->byteSize == 0)
		{
			if (m_readContext.pTilesTask && !m_readContext.pTilesTask->Update(static_cast<double>(tileIndex + 1)))
				return { false, "Operation was cancelled by user" };
			return { true, "" };
		}

		if (FileSystem::LFSFacade::FSeek(pFile, tileNode->offset, SEEK_SET) != 0)
			return { false, "Disk I/O Error: Cannot seek to map tile (file is corrupted?)" };

		const bool bCompressTiles = (m_readContext.header.compressedBlocks & static_cast<uint8>(ECompressedBlocks::Tiles)) != 0;

		auto tileProgress = [&](size_t bytesRead) -> bool {
			double fractional = static_cast<double>(bytesRead) / static_cast<double>(tileNode->byteSize);
			return m_readContext.pTilesTask ? m_readContext.pTilesTask->Update(static_cast<double>(tileIndex) + fractional) : true;
		};

		const uint32 tx = static_cast<uint32>(tileIndex % m_readContext.header.tileCountX);
		const uint32 ty = static_cast<uint32>(tileIndex / m_readContext.header.tileCountX);

		if (bCompressTiles)
		{
			if (auto result = decompressor.Begin(); !result.bSuccess)
				return result;

			if (auto result = decompressor.ProcessChunked(pFile, tileNode->byteSize, tileProgress); !result.bSuccess)
				return result;

			ImageWork::Converters::MapTileToImage(decompressor.decompressedBuffer, tx, ty, convertCtx);
		}
		else
		{
			if (tileNode->byteSize > tileBuffer.capacity())
				return { false, "Invalid tile size: exceeds maximum bounds" };

			if (auto result = Utils::FileSystem::ReadDataChunked(pFile, tileBuffer.data(), 1, tileNode->byteSize, tileProgress); !result.bSuccess)
				return result;

			ImageWork::Converters::MapTileToImage(tileBuffer, tx, ty, convertCtx);
		}

		if (m_readContext.pTilesTask && !m_readContext.pTilesTask->Update(static_cast<double>(tileIndex + 1)))
			return { false, "Operation was cancelled by user" };

		return { true, "" };
	}
}