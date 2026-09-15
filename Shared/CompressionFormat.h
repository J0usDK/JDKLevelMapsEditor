#pragma once
#include <type_traits>
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps
{
	enum class ECompressionAlg : uint8
	{
		None = 0,
		Zlib,
		Zstd,
		LZ4
	};

	enum class ECompressedBlocks : uint8
	{
		None = 0,
		Directory = 1 << 0,
		Tiles = 1 << 1
	};

	inline ECompressedBlocks operator&(ECompressedBlocks a, ECompressedBlocks b)
	{
		using T = std::underlying_type_t<ECompressedBlocks>;
		return static_cast<ECompressedBlocks>(static_cast<T>(a) & static_cast<T>(b));
	}

	inline ECompressedBlocks operator|(ECompressedBlocks a, ECompressedBlocks b)
	{
		using T = std::underlying_type_t<ECompressedBlocks>;
		return static_cast<ECompressedBlocks>(static_cast<T>(a) | static_cast<T>(b));
	}

	inline ECompressedBlocks operator&=(ECompressedBlocks& a, ECompressedBlocks b)
	{
		a = a & b;
		return a;
	}
	
	inline ECompressedBlocks operator|=(ECompressedBlocks& a, ECompressedBlocks b)
	{
		a = a | b;
		return a;
	}
}