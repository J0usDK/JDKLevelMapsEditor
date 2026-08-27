#pragma once
#include <vector>
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps
{
	enum class EMapType : uint8;
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

namespace JDKLevelMaps::Bakers
{
	class IMapBaker;
}

namespace JDKLevelMaps::Data
{
	struct SLevelContext;
	struct SRunResult;
}

namespace JDKLevelMaps::MapWork
{
	using BakeData = const std::vector<uint8>&;

	class CMapFileWriter
	{
	public:
		[[nodiscard]] Data::SRunResult Prepare(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor, BakeData bakingData);

		[[nodiscard]] Data::SRunResult BakeMap(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, BakeData bakingData);

	private:
		struct SBakeContext
		{
			Utils::Common::SProgressTask* pTilesTask = nullptr;
			Utils::Common::SProgressTask* pDirectoryTask = nullptr;

			bool bReady = false;

			EMapType mapType;
			bool bUseCompact;

			std::string mapPath;

			uint32 numChannels;
			uint32 tileCountX;
			uint32 tileCountY;
			uint64 totalTiles;

			uint64 directoryOffset;
			uint64 tilesOffset;
			uint64 nonEmptyTilesCount;

			std::vector<uint8> tilesOccupancy;
		};

		static constexpr uint32 kMaxChunkSize = 1024 * 1024;

	private:
		[[nodiscard]] Data::SRunResult BuildBakeContext(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, const Data::SLevelContext& context, BakeData bakingData);

		[[nodiscard]] Data::SRunResult ComputeTilesOccupancy(const Data::SLevelContext& context, BakeData bakingData);

		[[nodiscard]] bool WriteHeader(const Data::SLevelContext& context, Utils::FileSystem::ScopedCryFile& file);

		template<typename TTileEntry>
		[[nodiscard]] Data::SRunResult WriteMap(const Data::SLevelContext& context, BakeData bakingData);

		template<typename TTileEntry>
		[[nodiscard]] Data::SRunResult WriteTiles(const Data::SLevelContext& context, Utils::FileSystem::ScopedCryFile& file, BakeData bakingData, std::vector<TTileEntry>& directory);

		template<typename TTileEntry>
		[[nodiscard]] Data::SRunResult WriteTile(const Data::SLevelContext& context, Utils::FileSystem::ScopedCryFile& file, std::vector<uint8>& tileBuffer, std::vector<TTileEntry>& directory, BakeData bakingData, size_t tileIndex, uint32 tx, uint32 ty, uint64& nonEmptyProcessed);

		template<typename TTileEntry>
		[[nodiscard]] Data::SRunResult WriteDirectory(Utils::FileSystem::ScopedCryFile& file, std::vector<TTileEntry>& directory);

		template<typename TProgressCallback>
		[[nodiscard]] bool WriteDataChunked(Utils::FileSystem::ScopedCryFile& file, const void* pData, size_t elementSize, size_t totalElements, TProgressCallback progressCallback);

	private:
		SBakeContext m_bakeContext;
	};
}