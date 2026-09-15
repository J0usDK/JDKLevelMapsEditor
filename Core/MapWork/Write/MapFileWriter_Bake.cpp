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
	struct SCompressor final
	{
		const JDKLevelMaps::MapWork::Strategies::ICompressionStrategy* pStrategy = nullptr;
		std::unique_ptr<JDKLevelMaps::MapWork::Strategies::ICompressionContext> pContext = nullptr;

		std::vector<uint8> compressedBuffer;

		explicit SCompressor(const JDKLevelMaps::MapWork::Strategies::ICompressionStrategy* pStrategy) noexcept
			: pStrategy(pStrategy) { }

		[[nodiscard]] JDKLevelMaps::Data::SRunResult Begin() noexcept
		{
			if (!pStrategy)
				return { false, "Internal Error: Compressor not initialized" };

			pContext = pStrategy->BeginCompression();
			return pContext ? JDKLevelMaps::Data::SRunResult{ true, "" } : JDKLevelMaps::Data::SRunResult{ false, "Out of Memory: Failed to initialize compression context" };
		}

		[[nodiscard]] JDKLevelMaps::Data::SRunResult Process(const void* srcData, size_t srcSize) noexcept
		{
			return pStrategy && pContext ? pStrategy->CompressChunk(*pContext, srcData, srcSize, compressedBuffer) : JDKLevelMaps::Data::SRunResult{ false, "Internal Error: Compressor or Context not initialized" };
		}

		template<typename TCallback>
		[[nodiscard]] JDKLevelMaps::Data::SRunResult ProcessChunked(const void* srcData, size_t srcSize, TCallback&& callback)
		{
			const static constexpr size_t CHUNK_SIZE = 1024 * 1024;

			if (!pStrategy || !pContext)
				return { false, "Internal Error: Compressor or Context not initialized" };

			const uint8* pRaw = static_cast<const uint8*>(srcData);
			for (size_t offset = 0; offset < srcSize; offset += CHUNK_SIZE)
			{
				const size_t chunk = std::min(CHUNK_SIZE, srcSize - offset);

				if (auto result = pStrategy->CompressChunk(*pContext, pRaw + offset, chunk, compressedBuffer); !result.bSuccess)
					return result;

				if (!callback(chunk))
					return { false, "Operation cancelled by user" };
			}

			return { true, "" };
		}

		template<typename TCallback>
		[[nodiscard]] JDKLevelMaps::Data::SRunResult Flush(FILE* pFile, TCallback&& callback)
		{
			if (compressedBuffer.empty())
				return { true, "" };

			return JDKLevelMaps::Utils::FileSystem::WriteDataChunked(pFile, compressedBuffer.data(), 1, compressedBuffer.size(), callback);
		}

		[[nodiscard]] JDKLevelMaps::Data::SRunResult End() noexcept
		{
			return pStrategy && pContext ? pStrategy->EndCompression(*pContext, compressedBuffer) : JDKLevelMaps::Data::SRunResult{ false, "Internal Error: Compressor or Context not initialized" };
		}
	};

	template<typename TOffset, typename TCallback>
	struct SOffsetsRecorder final
	{
		std::vector<TOffset>& buffer;
		SCompressor* pCompressor;
		bool bCompress;
		TCallback progressCallback;

		[[nodiscard]] JDKLevelMaps::Data::SRunResult operator()(uint64 offset)
		{
			buffer.push_back(static_cast<TOffset>(offset));

			if (bCompress && buffer.size() == buffer.capacity())
			{
				const size_t bytes = buffer.size() * sizeof(TOffset);
				if (auto result = pCompressor->Process(buffer.data(), bytes); !result.bSuccess)
					return result;

				buffer.clear();

				if (!progressCallback(bytes))
					return { false, "Operation cancelled by user" };
			}
			return { true, "" };
		}

		[[nodiscard]] JDKLevelMaps::Data::SRunResult Finish()
		{
			if (bCompress && !buffer.empty())
				return pCompressor->Process(buffer.data(), buffer.size() * sizeof(TOffset));
			return { true, "" };
		}
	};

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
	bool CMapFileWriter::WriteHeader(const SBakeContext& context, FILE* pFile)
	{
		FileSystem::LFSFacade::FSeek(pFile, 0, SEEK_SET);

		SMapHeader header;
		header.mapType = m_writeContext.mapType;
		header.entryFormat = context.entryFormat;
		header.activeLayersMask = m_writeContext.layersMask;

		header.compressionAlg = context.compressAlg;
		header.compressedBlocks = context.compressBlocks;
		header.directoryOffset = m_directoryOffset;
		header.rawDirectorySize = m_rawDirectorySize;
		header.compressedDirectorySize = m_compressedDirectorySize;

		header.gridWidth = context.levelContext.gridWidth;
		header.gridHeight = context.levelContext.gridHeight;
		header.cellSize = context.levelContext.cellSize;
		header.originX = context.levelContext.originX;
		header.originY = context.levelContext.originY;
		header.tileSize = context.levelContext.tileSize;
		header.tileCountX = m_writeContext.tileCountX;
		header.tileCountY = m_writeContext.tileCountY;

		return gEnv->pCryPak->FWrite(&header, sizeof(JDKLevelMaps::SMapHeader), 1, pFile) == 1;
	}

	Data::SRunResult CMapFileWriter::WriteMap(const SBakeContext& context, FILE* pFile)
	{
		if (auto result = ProcessTiles(context, pFile, [](uint64) { return Data::SRunResult{ true, "" }; }); !result.bSuccess)
			return result;

		m_directoryOffset = FileSystem::LFSFacade::FTell(pFile);
		m_rawDirectorySize = m_writeContext.bitmask.size() * sizeof(uint64);
		m_compressedDirectorySize = 0;

		size_t processedRawBytes = 0;
		auto updateProgress = [&](size_t bytesAdded) -> bool {
			processedRawBytes += bytesAdded;
			return m_writeContext.pDirectoryTask ? m_writeContext.pDirectoryTask->Update(static_cast<double>(processedRawBytes)) : true;
		};

		const bool bCompressDir = (context.compressBlocks & static_cast<uint8>(ECompressedBlocks::Directory)) != 0;
		if (bCompressDir)
		{
			auto flushProgress = [&](size_t elementsWritten) -> bool {
				return m_writeContext.pDirectoryFlushTask ? m_writeContext.pDirectoryFlushTask->Update(static_cast<double>(elementsWritten)) : true;
			};

			SCompressor compressor(GetCompressor());
			
			if (auto result = compressor.Begin(); !result.bSuccess)
				return result;

			if (auto result = compressor.ProcessChunked(m_writeContext.bitmask.data(), m_writeContext.bitmask.size() * sizeof(uint64), updateProgress); !result.bSuccess)
				return result;

			if (auto result = compressor.End(); !result.bSuccess)
				return result;

			if (m_writeContext.pDirectoryFlushTask)
				m_writeContext.pDirectoryFlushTask->SetMaxValue(compressor.compressedBuffer.size());

			if (auto result = Utils::FileSystem::WriteDataChunked(pFile, compressor.compressedBuffer.data(), 1, compressor.compressedBuffer.size(), flushProgress); !result.bSuccess)
				return result;

			m_compressedDirectorySize += compressor.compressedBuffer.size();
		}
		else
		{
			auto rawProgress = [&](size_t elementsWritten) -> bool {
				processedRawBytes = elementsWritten * sizeof(uint64);
				return m_writeContext.pDirectoryTask ? m_writeContext.pDirectoryTask->Update(static_cast<double>(processedRawBytes)) : true;
			};

			if (auto result = Utils::FileSystem::WriteDataChunked(pFile, m_writeContext.bitmask.data(), sizeof(uint64), m_writeContext.bitmask.size(), rawProgress); !result.bSuccess)
				return result;
		}

		return { true, "" };
	}

	template<typename TOffset>
	Data::SRunResult CMapFileWriter::WriteMap(const SBakeContext& context, FILE* pFile)
	{
		const size_t bitmaskBytes = m_writeContext.bitmask.size() * sizeof(uint64);
		m_rawDirectorySize = bitmaskBytes + (m_writeContext.nonEmptyTilesCount + 1) * sizeof(TOffset);
		m_compressedDirectorySize = 0;

		size_t processedRawBytes = 0;
		auto updateProgress = [&](size_t bytesAdded) -> bool {
			processedRawBytes += bytesAdded;
			return m_writeContext.pDirectoryTask ? m_writeContext.pDirectoryTask->Update(static_cast<double>(processedRawBytes)) : true;
		};

		const bool bCompressDir = (context.compressBlocks & static_cast<uint8>(ECompressedBlocks::Directory)) != 0;
		SCompressor compressor(GetCompressor());

		std::vector<TOffset> rawOffsetsBuffer;

		if (bCompressDir)
		{
			if (auto result = compressor.Begin(); !result.bSuccess)
				return result;

			if (auto result = compressor.ProcessChunked(m_writeContext.bitmask.data(), bitmaskBytes, updateProgress); !result.bSuccess)
				return result;

			if (!Utils::Common::TryReserve(rawOffsetsBuffer, 1024 * 1024 / sizeof(TOffset)))
				return { false, "Out of Memory: Failed to allocate staging buffer" };
		}
		else
		{
			if (!Utils::Common::TryReserve(rawOffsetsBuffer, m_writeContext.nonEmptyTilesCount + 1))
				return { false, "Out of Memory: Failed to allocate offsets buffer" };
		}

		SOffsetsRecorder<TOffset, decltype(updateProgress)> recordOffsets{rawOffsetsBuffer, &compressor, bCompressDir, updateProgress };
		if (auto result = ProcessTiles(context, pFile, recordOffsets); !result.bSuccess)
			return result;

		if (auto result = recordOffsets.Finish(); !result.bSuccess)
			return result;

		m_directoryOffset = FileSystem::LFSFacade::FTell(pFile);

		if (bCompressDir)
		{
			if (auto result = compressor.End(); !result.bSuccess)
				return result;

			m_compressedDirectorySize = compressor.compressedBuffer.size();

			if (m_writeContext.pDirectoryFlushTask)
				m_writeContext.pDirectoryFlushTask->SetMaxValue(compressor.compressedBuffer.size());

			auto flushProgress = [&](size_t elementsWritten) -> bool {
				return m_writeContext.pDirectoryFlushTask ? m_writeContext.pDirectoryFlushTask->Update(static_cast<double>(elementsWritten)) : true;
			};

			if (auto result = compressor.Flush(pFile, flushProgress); !result.bSuccess)
				return result;
		}
		else
		{
			auto rawProgressBitmask = [&](size_t elementsWritten) -> bool {
				processedRawBytes = elementsWritten * sizeof(uint64);
				return m_writeContext.pDirectoryTask ? m_writeContext.pDirectoryTask->Update(static_cast<double>(processedRawBytes)) : true;
			};

			if (auto result = Utils::FileSystem::WriteDataChunked(pFile, m_writeContext.bitmask.data(), sizeof(uint64), m_writeContext.bitmask.size(), rawProgressBitmask); !result.bSuccess)
				return result;

			auto rawProgressOffsets = [&](size_t elementsWritten) -> bool {
				processedRawBytes = bitmaskBytes + elementsWritten * sizeof(TOffset);
				return m_writeContext.pDirectoryTask ? m_writeContext.pDirectoryTask->Update(static_cast<double>(processedRawBytes)) : true;
			};

			if (auto result = Utils::FileSystem::WriteDataChunked(pFile, rawOffsetsBuffer.data(), sizeof(TOffset), rawOffsetsBuffer.size(), rawProgressOffsets); !result.bSuccess)
				return result;
		}

		return { true, "" };
	}

	template<typename TOffsetRecorder>
	Data::SRunResult CMapFileWriter::ProcessTiles(const SBakeContext& context, FILE* pFile, TOffsetRecorder&& recordOffset)
	{
		const size_t rawTileSize = static_cast<size_t>(context.levelContext.tileSize) * context.levelContext.tileSize * m_writeContext.channelsCount;
		std::vector<uint8> tileBuffer;

		if (!Utils::Common::TryResize(tileBuffer, rawTileSize))
			return { false, "Out of Memory: Failed to allocate memory for tile buffer" };

		const bool bCompressTiles = (context.compressBlocks & static_cast<uint8>(ECompressedBlocks::Tiles)) != 0;
		const Strategies::ICompressionStrategy* pCompressor = nullptr;

		std::vector<uint8> compBuffer;
		size_t maxCompSize = 0;

		if (bCompressTiles)
		{
			pCompressor = GetCompressor();
			if (!pCompressor)
				return { false, "Internal Error: Compressor not initialized for tiles" };

			maxCompSize = pCompressor->GetMaxCompressedSize(rawTileSize);
			if (!Utils::Common::TryResize(compBuffer, maxCompSize))
				return { false, "Out of Memory: Failed to allocate memory for compression buffer" };
		}

		uint32 tx = 0, ty = 0;
		uint64 nonEmptyProcessed = 0;
		uint64 bytesProcessed = 0;

		FileSystem::LFSFacade::FSeek(pFile, m_writeContext.tilesOffset, SEEK_SET);

		for (size_t tileIndex = 0; tileIndex < m_writeContext.totalTiles; ++tileIndex)
		{
			const size_t blockIndex = tileIndex >> 6;
			const uint64 bitFlag = 1ULL << (tileIndex & 63);

			if ((m_writeContext.bitmask[blockIndex] & bitFlag) != 0)
			{
				ExtractTileData(context.bakingData, tx, ty, context.levelContext, m_writeContext.channelsCount, tileBuffer);

				const void* pWriteData = tileBuffer.data();
				size_t bytesToWrite = rawTileSize;

				if (bCompressTiles)
				{
					size_t compSize = 0;
					if (auto result = pCompressor->Compress(tileBuffer.data(), rawTileSize, compBuffer.data(), maxCompSize, compSize); !result.bSuccess)
						return result;

					pWriteData = compBuffer.data();
					bytesToWrite = compSize;
				}

				const uint64 currentOffset = JDKLevelMaps::FileSystem::LFSFacade::FTell(pFile);
				if (auto result = recordOffset(currentOffset); !result.bSuccess)
					return result;

				const auto updateProgress = [&](size_t writtenCompressed) {
					double ratio = bytesToWrite > 0 ? static_cast<double>(writtenCompressed) / static_cast<double>(bytesToWrite) : 1.0;
					double currentRaw = static_cast<double>(bytesProcessed) + ratio * static_cast<double>(rawTileSize);
					return m_writeContext.pTilesTask ? m_writeContext.pTilesTask->Update(currentRaw) : true;
				};

				if (auto result = Utils::FileSystem::WriteDataChunked(pFile, pWriteData, 1, bytesToWrite, updateProgress); !result.bSuccess)
					return result;

				bytesProcessed += tileBuffer.size();
				nonEmptyProcessed++;
			}

			if (++tx == m_writeContext.tileCountX)
				{ tx = 0; ++ty; }
		}

		const uint64 finalOffset = JDKLevelMaps::FileSystem::LFSFacade::FTell(pFile);
		if (auto result = recordOffset(finalOffset); !result.bSuccess)
			return result;

		return { true, "" };
	}
}