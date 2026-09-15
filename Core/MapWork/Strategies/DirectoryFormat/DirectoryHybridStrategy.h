#pragma once
#include "IDirectoryFormatStrategy.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	template<typename TOffset>
	class CDirectoryHybridStrategy final : public IDirectoryFormatStrategy
	{
	public:
		[[nodiscard]] Data::SRunResult ParseDirectory(const std::vector<uint8>& directoryData, const Data::SMapReadContext& context) override;

		[[nodiscard]] Data::SDirectoryInfo GetDirectoryInfo() const noexcept override;
		[[nodiscard]] std::optional<Data::STileNode> GetTileNode(uint64 tileIndex) const noexcept override;

	private:
		std::vector<TOffset> m_offsets;
	};
}