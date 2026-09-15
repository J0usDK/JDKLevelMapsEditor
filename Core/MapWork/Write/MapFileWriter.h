#pragma once
#include <vector>
#include <variant>

#include <CryCore/BaseTypes.h>

#include "Core/Data/MapContext.h"
#include "Core/MapWork/Strategies/DirectoryFormat/DirectoryBitmaskStrategy.h"
#include "Core/MapWork/Strategies/DirectoryFormat/DirectoryHybridStrategy.h"
#include "Core/MapWork/Strategies/Compression/ZlibCompressionStrategy.h"
#include "Core/MapWork/Strategies/Compression/ZstdCompressionStrategy.h"
#include "Core/MapWork/Strategies/Compression/LZ4CompressionStrategy.h"

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

namespace JDKLevelMaps::MapWork
{
	struct SBakeContext
	{
		const ECompressionAlg compressAlg = ECompressionAlg::None;
		uint8 compressBlocks = 0;
		ETileEntryFormat entryFormat = ETileEntryFormat::Bitmask;

		const Bakers::IMapBaker& baker;
		const Data::SLevelContext& levelContext;
		const std::vector<uint8>& bakingData;

		SBakeContext() = delete;
		SBakeContext(const Bakers::IMapBaker& baker, const Data::SLevelContext& levelContext, const std::vector<uint8>& bakingData, ECompressionAlg compressAlg)
			:baker(baker), levelContext(levelContext), bakingData(bakingData), compressAlg(compressAlg) { }
	};

	class CMapFileWriter
	{
	public:
		[[nodiscard]] Data::SRunResult Prepare(const SBakeContext& context, FileSystem::CPathResolver& pathResolver);

		[[nodiscard]] Data::SRunResult BakeMap(const SBakeContext& context, Utils::Common::CProgressor* pProgressor);

		// Valid only after Prepare()
		[[nodiscard]] uint64 GetNonEmptyTilesCount() const noexcept;

	private:
		// PREPARE PHASE

		[[nodiscard]] Data::SRunResult BuildWriteContext(const SBakeContext& context, FileSystem::CPathResolver& pathResolver);

		[[nodiscard]] Data::SRunResult ValidateBakeContext(const SBakeContext& context) const noexcept;

		[[nodiscard]] Data::SRunResult InitCompressStrategy(const SBakeContext& context) noexcept;

		void InitProgressTasks(const SBakeContext& context, Utils::Common::CProgressor* pProgressor);

		void InitLayout(const SBakeContext& context) noexcept;

		[[nodiscard]] Data::SRunResult ComputeTilesOccupancy(const SBakeContext& context);

		[[nodiscard]] Data::SRunResult ResolveMapPath(const SBakeContext& context, FileSystem::CPathResolver& pathResolver);

	private:
		// BAKE PHASE

		[[nodiscard]] bool WriteHeader(const SBakeContext& context, FILE* pFile);

		[[nodiscard]] Data::SRunResult WriteMap(const SBakeContext& context, FILE* pFile);

		template<typename TOffset>
		[[nodiscard]] Data::SRunResult WriteMap(const SBakeContext& context, FILE* pFile);

		template<typename TOffsetRecorder>
		[[nodiscard]] Data::SRunResult ProcessTiles(const SBakeContext& context, FILE* pFile, TOffsetRecorder&& recordOffset);

	private:
		// HELPERS

		[[nodiscard]] const Strategies::ICompressionStrategy* GetCompressor() const noexcept;
		
	private:
		using TZlibCompressStrategy		= Strategies::CZlibCompressionStrategy;
		using TZstdCompressStrategy		= Strategies::CZstdCompressionStrategy;
		using TLZ4CompressStrategy		= Strategies::CLZ4CompressionStrategy;

	private:
		Data::SMapWriteContext m_writeContext;

		std::variant<std::monostate, TZlibCompressStrategy, TZstdCompressStrategy, TLZ4CompressStrategy> m_compressStrategy;

		uint64 m_directoryOffset = 0;
		uint64 m_rawDirectorySize = 0;
		uint64 m_compressedDirectorySize = 0;

		bool m_bReady = false;
	};
}