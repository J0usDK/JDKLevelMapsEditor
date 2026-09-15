#include "StdAfx.h"
#include "Progress.h"

namespace JDKLevelMaps::Utils::Common
{
	CProgressor::CProgressor(SProgress& progress) noexcept : m_progress(progress) { }

	SProgressTask* CProgressor::RegisterProgressTask(size_t totalOperations)
	{
		if (totalOperations == 0)
			return nullptr;

		m_progressContexts.push_back(std::make_unique<SProgressTask>(totalOperations, m_progress.progress, m_progress.bIsCancelled));
		RecalculateContexts();

		return m_progressContexts.back().get();
	}

	SProgressTask* CProgressor::ReserveProgressTask(uint8 percent)
	{
		if (percent == 0)
			return nullptr;

		m_progressContexts.push_back(std::make_unique<SProgressTask>(percent, m_progress.progress, m_progress.bIsCancelled));
		RecalculateContexts();

		return m_progressContexts.back().get();
	}

	bool CProgressor::IsCancelled() const noexcept
	{
		return m_progress.bIsCancelled.load(std::memory_order_relaxed);
	}

	void CProgressor::RecalculateContexts()
	{
		double totalWork = 0.0;
		uint32 reservedProgress = 0;

		for (const auto& context : m_progressContexts)
		{
			if (context->reservedProgress != 0)
				reservedProgress += context->reservedProgress;
			else
				totalWork += static_cast<double>(context->totalOperations);
		}

		CRY_ASSERT(reservedProgress <= 100);
		const double availableProgress = 100.0 - static_cast<double>(reservedProgress);

		for (const auto& context : m_progressContexts)
		{
			if (context->reservedProgress != 0)
				context->progressFraction = static_cast<double>(context->reservedProgress) / 100.0;
			else if (totalWork > 0.0)
			{
				context->progressFraction = (static_cast<double>(context->totalOperations) / totalWork) * (availableProgress / 100.0);
			}
			else
				context->progressFraction = 0.0;
		}
	}
}