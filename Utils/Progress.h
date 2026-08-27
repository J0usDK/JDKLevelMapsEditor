#pragma once
#include <string>
#include <atomic>

#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps::Utils::Common
{
	struct SProgress
	{
		// Safe to use std::memory_order_relaxed for both load() and fetch_add()
		std::atomic<int> progress{ 0 };

		// bIsCompleted is the synchronization flag for resultMessage and bIsSuccess.
		// - WRITE: Use std::memory_order_release AFTER string and status.
		// - READ:	Use std::memory_order_acquire BEFORE reading string or status.
		std::atomic<bool> bIsCompleted{ false };

		// Safe to use std::memory_order_relaxed for both load() and store()
		std::atomic<bool> bIsCancelled{ false };

		// Safe to use std::memory_order_relaxed for both load() and store() IF protected by 'isCompleted'
		std::atomic<bool> bIsSuccess{ false };

		std::string resultMessage;

		void Reset() noexcept
		{
			progress.store(0);
			bIsCompleted.store(false);
			bIsCancelled.store(false);
			bIsSuccess.store(false);
			resultMessage.clear();
		}
	};

	class CProgressor;

	struct SProgressTask
	{
	public:
		SProgressTask() = delete;
		SProgressTask(size_t totalOps, size_t weight, std::atomic<int>& progressRef, std::atomic<bool>& bCancelledRef) noexcept
			: totalOperations(totalOps), operationWeight(weight), progress(progressRef), bCancelled(bCancelledRef) {
		}

		// Returns false if operation was cancelled
		[[nodiscard]] bool Update(double currentOperation) noexcept
		{
			CRY_ASSERT(std::isfinite(currentOperation));
			CRY_ASSERT(currentOperation <= static_cast<double>(totalOperations));

			if (bCancelled.load(std::memory_order_relaxed))
				return false;

			const double localRatio = currentOperation / static_cast<double>(totalOperations);
			const double currentGlobalContribution = localRatio * progressRange * 100.0;
			const int currentInt = static_cast<int>(currentGlobalContribution);
			const int delta = currentInt - m_lastReportedContribution;

			if (delta > 0)
			{
				progress.fetch_add(delta, std::memory_order_relaxed);
				m_lastReportedContribution = currentInt;
			}

			return true;
		}

	private:
		friend class CProgressor;

		double progressRange = 0.0;
		int m_lastReportedContribution = 0;

		const size_t totalOperations = 0;
		const size_t operationWeight = 0;

		std::atomic<int>& progress;
		std::atomic<bool>& bCancelled;
	};

	class CProgressor
	{
	public:
		CProgressor() = delete;
		explicit CProgressor(SProgress& progress) noexcept;
		~CProgressor() = default;

		// Register all tasks before progress updates begin for accurate progress weighting.
		[[nodiscard]] SProgressTask* RegisterProgressTask(size_t totalOperations, size_t operationWeight);

		[[nodiscard]] bool IsCancelled() const noexcept;

	private:
		void RecalculateContexts();

	private:
		SProgress& m_progress;
		std::vector<std::unique_ptr<SProgressTask>> m_progressContexts;
	};
}