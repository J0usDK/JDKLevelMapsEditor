#pragma once
#include <CryCore/BaseTypes.h>

#include "Shared/CompressionFormat.h"

namespace JDKLevelMaps::Settings
{
	enum class ECompression : uint8
	{
		None = 0,
		Auto,
		ZlibMetadata,
		ZstdMetadata,
		LZ4Metadata,
		ZlibData,
		ZstdData,
		LZ4Data,
		ZlibBoth,
		ZstdBoth,
		LZ4Both
	};

	enum class EDirectoryFormat : uint8
	{
		Auto = 0,
		Bitmask = 1,
		Hybrid = 2
	};

	struct SBakerSettings
	{
		float cellSize = 1.0f;
		uint32 tileSize = 64;

		ECompression compression = ECompression::None;
		EDirectoryFormat directoryFormat = EDirectoryFormat::Bitmask;
		bool bGenerateDebugImage = false;
	};
}