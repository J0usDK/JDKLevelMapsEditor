#include "StdAfx.h"
#include "MapFileReader.h"

#include "Core/Data/RunResult.h"
#include "Core/ImageWork/MapImageConverter.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/VectorUtils.h"
#include "Utils/Progress.h"

namespace JDKLevelMaps::MapWork
{
	[[nodiscard]] Data::SRunResult CMapFileReader::ReadTiles(FILE* pFile, ImageWork::SImageView& outImage)
	{
		ImageWork::Converters::SConvertContext convertCtx(m_readContext.header, m_readContext.channelsCount, m_readContext.header.activeLayersMask, outImage, m_readContext.pColorMapper);

		Data::SRunResult dirResult = std::visit([&](auto&& strategy) -> Data::SRunResult
			{
				using T = std::decay_t<decltype(strategy)>;
				if constexpr (std::is_same_v<T, std::monostate>)
					return { false, "Internal Error: Strategy is not initialized" };
				else
					return strategy.ReadDirectory(pFile, m_readContext);
			}, m_strategy);

		if (!dirResult.bSuccess)
			return dirResult;

		const size_t maxTileSize = static_cast<size_t>(m_readContext.header.tileSize) * m_readContext.header.tileSize * m_readContext.channelsCount;
		std::vector<uint8> tileBuffer;
		if (!Utils::Common::TryResize(tileBuffer, maxTileSize))
			return { false, "Out of Memory: Failed to allocate memory for tile buffer during map reading" };

		const uint64 totalTiles = static_cast<uint64>(m_readContext.header.tileCountX) * m_readContext.header.tileCountY;

		for (uint64 i = 0; i < totalTiles; ++i)
		{
			if (auto result = ReadTile(i, pFile, tileBuffer, convertCtx); !result.bSuccess)
				return result;

			if (m_readContext.pReadTask && !m_readContext.pReadTask->Update(i + 1))
				return { false, "Preview loading was cancelled by user" };
		}

		return { true, "" };
	}

	inline Data::SRunResult CMapFileReader::ReadTile(uint64 tileIndex, FILE* pFile, std::vector<uint8>& tileBuffer, ImageWork::Converters::SConvertContext& convertCtx)
	{
		const auto tileNode = std::visit([tileIndex](auto&& strategy) -> std::optional<Data::STileNode>
			{
				using T = std::decay_t<decltype(strategy)>;
				if constexpr (std::is_same_v<T, std::monostate>)
					return std::nullopt;
				else
					return strategy.GetTileNode(tileIndex);
			}, m_strategy);

		if (!tileNode.has_value() || tileNode->byteSize == 0)
			return { true, "" };

		if (tileNode->byteSize > tileBuffer.size())
			return { false, "Invalid tile size (file is corrupted?)" };

		if (FileSystem::LFSFacade::FSeek(pFile, tileNode->offset, SEEK_SET) != 0)
			return { false, "Disk I/O Error: Cannot seek to map tile (file is corrupted?)" };

		if (gEnv->pCryPak->FReadRaw(tileBuffer.data(), tileNode->byteSize, 1, pFile) != 1)
			return { false, "Disk I/O Error: Cannot read map's tiles (file is corrupted?)" };

		const uint32 tx = static_cast<uint32>(tileIndex % m_readContext.header.tileCountX);
		const uint32 ty = static_cast<uint32>(tileIndex / m_readContext.header.tileCountX);

		ImageWork::Converters::MapTileToImage(tileBuffer, tx, ty, convertCtx);

		return { true, "" };
	}
}