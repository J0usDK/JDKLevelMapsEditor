#pragma once
#include "Core/Bakers/IMapBaker.h"

namespace JDKLevelMaps::Data
{
	struct SLevelContext;
}

namespace JDKLevelMaps::MapLayers
{
	enum class EVegetationLayers : uint8;
}

namespace JDKLevelMaps::Settings
{
	struct SVegetationBakerSettings;
}

namespace JDKLevelMaps::Bakers
{
	class CVegetationBaker final : public Bakers::IMapBaker
	{
	public:
		explicit CVegetationBaker(const Settings::SVegetationBakerSettings& settings) noexcept;

		[[nodiscard]] const char* GetID() const noexcept override;
		[[nodiscard]] EMapType GetMapType() const noexcept override;
		[[nodiscard]] uint32 GetChannelCount() const noexcept override;

		[[nodiscard]] std::vector<uint8> Bake(const Data::SLevelContext& context) const override;

		[[nodiscard]] SDebugColor GetDebugColor(const uint8* pCellData) const noexcept override;

		[[nodiscard]] DebugColorMapperPtr GetDebugColorMapper() const noexcept override;

	private:
		const Settings::SVegetationBakerSettings& m_settings;
	};
}