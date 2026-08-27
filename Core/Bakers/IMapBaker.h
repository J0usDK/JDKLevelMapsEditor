#pragma once
#include <vector>
#include "Shared/MapHeader.h"

namespace JDKLevelMaps::Data
{
	struct SLevelContext;
}

namespace JDKLevelMaps::Bakers
{
	struct [[nodiscard]] SDebugColor
	{
		uint8 r = 0, g = 0, b = 0;
		constexpr SDebugColor(uint8 r, uint8 g, uint8 b) noexcept : r(r), g(g), b(b) {}
	};

	using DebugColorMapperPtr = SDebugColor(*)(const uint8*);

	class IMapBaker
	{
	public:
		virtual ~IMapBaker() = default;

		// Returns the baker's identifier
		[[nodiscard]] virtual const char* GetID() const noexcept = 0;
		[[nodiscard]] virtual EMapType GetMapType() const noexcept = 0;
		[[nodiscard]] virtual uint32 GetChannelCount() const noexcept { return 1; }

		[[nodiscard]] virtual std::vector<uint8> Bake(const Data::SLevelContext& context) const = 0;

		// Returns color of the map's cell
		[[nodiscard]] virtual SDebugColor GetDebugColor(const uint8* pCellData) const noexcept = 0;

		// Returns GetDebugColor function to prevent virtual call
		[[nodiscard]] virtual DebugColorMapperPtr GetDebugColorMapper() const noexcept = 0;
	};
}