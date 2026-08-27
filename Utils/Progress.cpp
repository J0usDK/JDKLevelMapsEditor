#include "StdAfx.h"
#include "Progress.h"

namespace JDKLevelMaps::Utils::Common
{
	CProgressor::CProgressor(SProgress& progress) noexcept : m_progress(progress) { }

	SProgressTask* CProgressor::RegisterProgressTask(size_t totalOperations, size_t operationWeight)
	{
		if (totalOperations == 0 || operationWeight == 0)
			return nullptr;

		m_progressContexts.push_back(std::make_unique<SProgressTask>(totalOperations, operationWeight, m_progress.progress, m_progress.bIsCancelled));
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

		for (const auto& context : m_progressContexts)
			totalWork += static_cast<double>(context->totalOperations) * static_cast<double>(context->operationWeight);

		for (const auto& context : m_progressContexts)
		{
			if (totalWork > 0.0)
			{
				const double stageWork = static_cast<double>(context->totalOperations) * static_cast<double>(context->operationWeight);
				context->progressRange = stageWork / totalWork;
			}
			else
				context->progressRange = 0.0;
		}
	}
}