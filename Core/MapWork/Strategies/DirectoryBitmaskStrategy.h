#pragma once
#include "IDirectoryFormatStrategy.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	class CDirectoryBitmaskStrategy final : public IDirectoryFormatStrategy
	{
	public:
		Data::SRunResult WriteDirectory(FILE* pFile, const Data::SMapWriteContext& context, const void*) override;
		Data::SRunResult ReadDirectory(FILE* pFile, const Data::SMapReadContext& context) override;

		Data::SDirectoryInfo GetDirectoryInfo() const noexcept override;
		std::optional<Data::STileNode> GetTileNode(uint64 tileIndex) const noexcept override;

	private:
		uint64 m_baseDataOffset = 0;
		uint64 m_tileSizeBytes = 0;
 	};
}