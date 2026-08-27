#pragma once
#include <array>
#include <memory>
#include <utility>

#include "Shared/MapHeader.h"

namespace JDKLevelMaps::Bakers
{
	class IMapBaker;
}

namespace JDKLevelMaps::Managers
{
	class CBakersRegistry
	{
	public:
		~CBakersRegistry() noexcept;

		void RegisterBaker(std::unique_ptr<Bakers::IMapBaker> pBaker) noexcept;
		[[nodiscard]] const Bakers::IMapBaker* GetBaker(EMapType bakerType) const noexcept;

		template<typename TVisitor>
		void ForEachBaker(TVisitor&& visitor) const
			noexcept(std::is_nothrow_invocable_v<TVisitor, const Bakers::IMapBaker*>)
		{
			for (const auto& pBaker : m_bakers)
				if (pBaker)
					std::forward<TVisitor>(visitor)(pBaker.get());
		}

	private:
		const Bakers::IMapBaker* FindBakerInternal(EMapType bakerType) const noexcept;
		Bakers::IMapBaker* FindBakerInternal(EMapType bakerType) noexcept;

	private:
		std::array<std::unique_ptr<Bakers::IMapBaker>, static_cast<size_t>(EMapType::Count)> m_bakers;
	};
}