#pragma once
#include "IDirectoryFormatStrategy.h"

namespace JDKLevelMaps::MapWork::Strategies
{
	template<typename TOffset>
	class CDirectoryHybridStrategy final : public IDirectoryFormatStrategy
	{
	public:
		Data::SRunResult WriteDirectory(FILE* pFile, const Data::SMapWriteContext& context, const void* pOffsets) override;
		Data::SRunResult ReadDirectory(FILE* pFile, const Data::SMapReadContext& context) override;

		Data::SDirectoryInfo GetDirectoryInfo() const noexcept override;
		std::optional<Data::STileNode> GetTileNode(uint64 tileIndex) const noexcept override;
	
	private:
		Data::SRunResult WriteBitmask(FILE* pFile, const Data::SMapWriteContext& context);
		Data::SRunResult WriteOffsets(FILE* pFile, const Data::SMapWriteContext& context, const void* pOffsets);

	private:
		std::vector<TOffset> m_offsets;
	};
}