#pragma once
#include <vector>
#include <variant>

#include <CryCore/BaseTypes.h>

#include "Core/Data/MapContext.h"
#include "Core/MapWork/Strategies/DirectoryBitmaskStrategy.h"
#include "Core/MapWork/Strategies/DirectoryHybridStrategy.h"

namespace JDKLevelMaps
{
	enum class EMapType : uint8;
	enum class ETileEntryFormat : uint8;
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

namespace JDKLevelMaps::Bakers
{
	class IMapBaker;
}

namespace JDKLevelMaps::Data
{
	struct SLevelContext;
	struct SRunResult;
}

namespace JDKLevelMaps::MapWork::Strategies
{
	class IDirectoryFormatStrategy;
}

namespace JDKLevelMaps::MapWork
{
	using BakeData = const std::vector<uint8>&;

	class CMapFileWriter
	{
	public:
		[[nodiscard]] Data::SRunResult Prepare(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor, BakeData bakingData, bool bUseHybrid);

		[[nodiscard]] Data::SRunResult BakeMap(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, BakeData bakingData);

	private:
		// PREPARE PHASE

		[[nodiscard]] Data::SRunResult BuildBakeContext(const Bakers::IMapBaker& baker, FileSystem::CPathResolver& pathResolver, const Data::SLevelContext& context, BakeData bakingData, bool bUseHybrid);

		[[nodiscard]] Data::SRunResult ValidateBakeContext(const Data::SLevelContext& context) const noexcept;

		[[nodiscard]] Data::SRunResult InitDirectoryStrategy() noexcept;

		void InitProgressTasks(const Data::SLevelContext& context, Utils::Common::CProgressor* pProgressor);

		void InitTileLayout(const Data::SLevelContext& context) noexcept;

		void InitFileLayout(const Data::SLevelContext& context, EMapType mapType, bool bUseHybrid) noexcept;

		[[nodiscard]] Data::SRunResult ComputeTilesOccupancy(const Data::SLevelContext& context, BakeData bakingData);

		[[nodiscard]] ETileEntryFormat DetermineEntryFormat(const Data::SLevelContext& context, bool bUseHybrid) const noexcept;

		[[nodiscard]] uint64 CalculateSerializedSize(const Data::SLevelContext& context, uint64 offsetSize) const noexcept;

		[[nodiscard]] uint64 CalculateDataOffset(ETileEntryFormat format, uint64 nonEmptyTilesCount) const noexcept;

		[[nodiscard]] Data::SRunResult ResolveMapPath(FileSystem::CPathResolver& pathResolver, const Bakers::IMapBaker& baker);
		
		// BAKE PHASE

		[[nodiscard]] bool WriteHeader(const Data::SLevelContext& context, FILE* pFile);

		[[nodiscard]] Data::SRunResult WriteMap(const Data::SLevelContext& context, FILE* pFile, BakeData bakingData);

		template<typename TOffset>
		[[nodiscard]] Data::SRunResult WriteMap(const Data::SLevelContext& context, FILE* pFile, BakeData bakingData);

		template<typename TOffsetRecorder>
		[[nodiscard]] Data::SRunResult ProcessTilesLoop(const Data::SLevelContext& context, FILE* pFile, BakeData bakingData, TOffsetRecorder&& recordOffset);
		
	private:
		using TBitmaskStrategy = Strategies::CDirectoryBitmaskStrategy;
		using THybrid32Strategy = Strategies::CDirectoryHybridStrategy<uint32>;
		using THybrid64Strategy = Strategies::CDirectoryHybridStrategy<uint64>;

	private:
		Data::SMapWriteContext m_bakeContext;
		std::variant<std::monostate, TBitmaskStrategy, THybrid32Strategy, THybrid64Strategy> m_strategy;

		bool m_bReady = false;
	};
}