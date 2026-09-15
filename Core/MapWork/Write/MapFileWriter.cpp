#include "StdAfx.h"
#include "MapFileWriter.h"

#include <utility>

#include "Core/Data/LevelContext.h"
#include "Core/Data/RunResult.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/ScopedCryFile.h"

namespace JDKLevelMaps::MapWork
{
	Data::SRunResult CMapFileWriter::Prepare(const SBakeContext& context, FileSystem::CPathResolver& pathResolver)
	{
		if (auto result = InitCompressStrategy(context); !result.bSuccess)
			return result;

		if (auto result = BuildWriteContext(context, pathResolver); !result.bSuccess)
			return result;

		m_bReady = true;
		return { true, "" };
	}

	Data::SRunResult CMapFileWriter::BakeMap(const SBakeContext& context, Utils::Common::CProgressor* pProgressor)
	{
		if (!std::exchange(m_bReady, false))
			return { false, "Internal Error: Map Writer is not ready" };

		InitProgressTasks(context, pProgressor);

		Utils::FileSystem::ScopedCryFile file(FileSystem::LFSFacade::FOpen(m_writeContext.mapPath.c_str(), "wb", true), m_writeContext.mapPath.c_str());
		if (!file)
			return { false, "Disk I/O Error: Cannot open map file for writing" };

		Data::SRunResult result;
		switch (context.entryFormat)
		{
		case ETileEntryFormat::Bitmask:
			result = WriteMap(context, file);
			break;
		case ETileEntryFormat::Hybrid_32:
			result = WriteMap<uint32>(context, file);
			break;
		case ETileEntryFormat::Hybrid_64:
			result = WriteMap<uint64>(context, file);
			break;
		}

		if (!result.bSuccess)
			return result;

		if (!WriteHeader(context, file))
			return { false, "Disk I/O Error: Cannot write map's header" };

		file.close(true);
		return { true, "Map was written to: " + m_writeContext.mapPath };
	}

	uint64 CMapFileWriter::GetNonEmptyTilesCount() const noexcept
	{
		return m_writeContext.nonEmptyTilesCount;
	}

	const Strategies::ICompressionStrategy* CMapFileWriter::GetCompressor() const noexcept
	{
		return std::visit([](auto&& strategy) -> const Strategies::ICompressionStrategy* {
			using T = std::decay_t<decltype(strategy)>;
			if constexpr (std::is_same_v<T, std::monostate>)
				return nullptr;
			else return &strategy;
		}, m_compressStrategy);
	}
}