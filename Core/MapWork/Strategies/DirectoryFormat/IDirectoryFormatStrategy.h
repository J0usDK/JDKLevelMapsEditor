#pragma once
#include <vector>
#include <optional>

#include <CryCore/BaseTypes.h>

#include <Includes/JDKMath/UniversalRankTable.h>

namespace JDKLevelMaps::Data
{
	struct SMapWriteContext;
	struct SMapReadContext;
	struct SRunResult;

	struct SDirectoryInfo
	{
		uint64 nonEmptyTilesCount = 0;
		uint64 totalDirectorySize = 0;
	};

	struct STileNode
	{
		uint64 offset = 0;
		uint64 byteSize = 0;
	};

	struct SDirectorySection
	{
		const void* pData = nullptr;
		size_t bytes = 0;
	};
}

namespace JDKLevelMaps::MapWork::Strategies
{
	class IDirectoryFormatStrategy
	{
	public:
		virtual ~IDirectoryFormatStrategy() = default;

		[[nodiscard]] virtual Data::SRunResult ParseDirectory(const std::vector<uint8>& directoryData, const Data::SMapReadContext& context) = 0;

		[[nodiscard]] virtual Data::SDirectoryInfo GetDirectoryInfo() const noexcept = 0;
		[[nodiscard]] virtual std::optional<Data::STileNode> GetTileNode(uint64 tileIndex) const noexcept = 0;

	protected:
		[[nodiscard]] Data::SRunResult BuildRankTable() noexcept;
		[[nodiscard]] std::optional<uint64> GetTileRank(uint64 tileIndex) const noexcept;
		[[nodiscard]] uint64 GetTotalRank() const noexcept;

	protected:
		Data::SDirectoryInfo m_directoryInfo;
		std::vector<uint64> m_bitmask;

	private:
		JDK::Math::CUniversalRankTable m_rankTable;
	};
}