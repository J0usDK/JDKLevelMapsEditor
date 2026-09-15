#pragma once
#include "IDirectoryFormatStrategy.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	class CDirectoryBitmaskStrategy final : public IDirectoryFormatStrategy
	{
	public:
		[[nodiscard]] Data::SRunResult ParseDirectory(const std::vector<uint8>& directoryData, const Data::SMapReadContext& context) override;

		Data::SDirectoryInfo GetDirectoryInfo() const noexcept override;
		std::optional<Data::STileNode> GetTileNode(uint64 tileIndex) const noexcept override;

	private:
		uint64 m_baseDataOffset = 0;
		uint64 m_tileSizeBytes = 0;
 	};
}