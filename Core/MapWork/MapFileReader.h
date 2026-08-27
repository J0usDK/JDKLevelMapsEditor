#pragma once
#include "Shared/MapHeader.h"
#include "Core/Bakers/IMapBaker.h"

namespace JDKLevelMaps::Data
{
	struct SRunResult;
}

namespace JDKLevelMaps::FileSystem
{
	class CPathResolver;
}

namespace JDKLevelMaps::Utils::Common
{
	class CProgressor;
	struct SProgressTask;
}

namespace JDKLevelMaps::Utils::FileSystem
{
	struct ScopedCryFile;
}

namespace JDKLevelMaps::ImageWork
{
	struct SImageView;
}

namespace JDKLevelMaps::ImageWork::Converters
{
	struct SConvertContext;
}

namespace JDKLevelMaps::MapWork
{
	class CMapFileReader final
	{
	public:
		[[nodiscard]] Data::SRunResult Prepare(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor);

		[[nodiscard]] Data::SRunResult LoadPreviewFromMap(ImageWork::SImageView& outImage);

	private:
		Data::SRunResult BuildContext(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver);
		Data::SRunResult ReadMapHeader(Utils::FileSystem::ScopedCryFile& file);

		template<typename TTileEntry>
		[[nodiscard]] Data::SRunResult ReadTiles(Utils::FileSystem::ScopedCryFile& dirFile, ImageWork::SImageView& outImage);
	
	private:
		struct SReadContext
		{
			Utils::Common::SProgressTask* pReadTask = nullptr;
			Bakers::DebugColorMapperPtr pColorMapper = nullptr;

			SMapHeader mapHeader;
			std::string mapPath;
			uint32 channelsCount = 0;

			bool bReady = false;
		};

	private:
		inline Data::SRunResult PrepareTileBuffer(std::vector<uint8>& tileBuffer) const;

		template<typename TTileEntry>
		inline Data::SRunResult ReadTileChunk(FILE* dirFile, FILE* tileFile, std::vector<TTileEntry>& dirChunk, std::vector<uint8>& tileBuffer, uint64 tilesRead, size_t tileCount, ImageWork::Converters::SConvertContext& convertCtx);

		template<typename TTileEntry>
		inline Data::SRunResult ReadTile(const TTileEntry& entry, uint64 tileIndex, FILE* pFile, std::vector<uint8>& tileBuffer, ImageWork::Converters::SConvertContext& convertCtx);

	private:
		SReadContext m_readContext;

		static constexpr size_t kDirectoryChunkSize = 4096;
	};
}