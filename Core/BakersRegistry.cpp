#include "StdAfx.h"
#include "BakersRegistry.h"

#include "Bakers/IMapBaker.h"

namespace JDKLevelMaps::Managers
{
	CBakersRegistry::~CBakersRegistry() noexcept = default;

	void CBakersRegistry::RegisterBaker(std::unique_ptr<Bakers::IMapBaker> pBaker) noexcept
	{
		if (!pBaker)
			return;

		const size_t index = static_cast<size_t>(pBaker->GetMapType());
		CRY_ASSERT(index < m_bakers.size());

		if (index >= m_bakers.size() || m_bakers[index])
			return;

		m_bakers[index] = std::move(pBaker);
	}

	const Bakers::IMapBaker* CBakersRegistry::GetBaker(EMapType bakerType) const noexcept
	{
		return FindBakerInternal(bakerType);
	}

	const Bakers::IMapBaker* CBakersRegistry::FindBakerInternal(EMapType bakerType) const noexcept
	{
		const size_t index = static_cast<size_t>(bakerType);

		if (index >= m_bakers.size())
			return nullptr;
		return m_bakers[index].get();
	}

	Bakers::IMapBaker* CBakersRegistry::FindBakerInternal(EMapType bakerType) noexcept
	{
		const size_t index = static_cast<size_t>(bakerType);

		if (index >= m_bakers.size())
			return nullptr;
		return m_bakers[index].get();
	}
}