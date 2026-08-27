#pragma once
#include <cmath>
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps
{
	constexpr uint8 kLayerMapVersion = 3;
	constexpr uint32 kLayerMapMagic = 0x4A444B4D;

	enum class EMapType : uint8
	{
		VegetationDensity = 0,

		Count
	};

	enum class ETileEntryFormat : uint8
	{
		Compact_32 = 0,	// offset: uint32, size: uint32 (8 bytes)
		Standard_64 = 1	// offset: uint64, size: uint64 (16 bytes)
	};

#pragma pack(push, 1)
	struct STileEntry32
	{
		uint32 fileOffset = 0;
		uint32 byteSize = 0;
	};

	struct STileEntry64
	{
		uint64 fileOffset = 0;
		uint64 byteSize = 0;
	};

	struct SMapHeader
	{
		uint32 magic = kLayerMapMagic;
		uint8 version = kLayerMapVersion;
		EMapType mapType = EMapType::VegetationDensity;
		ETileEntryFormat entryFormat = ETileEntryFormat::Compact_32;
		uint8 reserved = 0; // currently not used, added for alignment

		int32 gridWidth = 0;
		int32 gridHeight = 0;
		float cellSize = 0.0f;	//meters per cell
		float originX = 0.0f;
		float originY = 0.0f;

		uint32 tileSize = 0;	//size in cells
		uint32 tileCountX = 0;
		uint32 tileCountY = 0;
	};
#pragma pack(pop)

	static_assert(sizeof(STileEntry32) == 8,
		"The size of STileEntry32 has been changed. Keep the in-game reader up to date.");

	static_assert(sizeof(STileEntry64) == 16,
		"The size of STileEntry64 has been changed. Keep the in-game reader up to date.");

	static_assert(sizeof(SMapHeader) == 40,
		"The size of SMapHeader has been changed. Keep the in-game reader up to date.");

	static_assert(std::is_trivially_copyable_v<STileEntry32>,
		"STileEntry32 must be trivially copyable for binary I/O");

	static_assert(std::is_trivially_copyable_v<STileEntry64>,
		"STileEntry64 must be trivially copyable for binary I/O");

	static_assert(std::is_trivially_copyable_v<SMapHeader>,
		"SMapHeader must be trivially copyable for binary I/O");

	[[nodiscard]] inline constexpr bool IsValidMapType(EMapType type) noexcept
	{
		return type < EMapType::Count;
	}

	[[nodiscard]] inline constexpr bool IsValidTileEntryFormat(ETileEntryFormat format) noexcept
	{
		return format == ETileEntryFormat::Compact_32 || format == ETileEntryFormat::Standard_64;
	}

	[[nodiscard]] inline constexpr bool IsValidMapHeader(const SMapHeader& header) noexcept
	{
		if (header.magic != kLayerMapMagic) return false;
		if (header.version != kLayerMapVersion) return false;
		if (!IsValidMapType(header.mapType)) return false;
		if (!IsValidTileEntryFormat(header.entryFormat)) return false;
		if (header.reserved != 0) return false;
		if (header.gridWidth <= 0 || header.gridHeight <= 0) return false;
		if (!std::isfinite(header.cellSize) || header.cellSize <= 0) return false;
		if (header.tileSize == 0) return false;

		const uint32 expectedTileCountX = static_cast<uint32>((static_cast<uint64>(header.gridWidth) + header.tileSize - 1) / header.tileSize);
		const uint32 expectedTileCountY = static_cast<uint32>((static_cast<uint64>(header.gridHeight) + header.tileSize - 1) / header.tileSize);
		
		if (header.tileCountX != expectedTileCountX || header.tileCountY != expectedTileCountY) return false;

		return true;
	}
}