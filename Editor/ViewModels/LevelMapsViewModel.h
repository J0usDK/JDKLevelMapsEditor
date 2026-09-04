#pragma once
#include <memory>
#include <thread>
#include <QObject>

#include <CryCore/BaseTypes.h>

#include "Utils/Progress.h"

class QString;
class QImage;

namespace JDKLevelMaps
{
	enum class EMapType : uint8;
}

namespace JDKLevelMaps::Settings
{
	struct SBakerSettings;
}

namespace JDKLevelMaps::FileSystem
{
	class CPathResolver;
}

namespace JDKLevelMaps::Managers
{
	class CBakersRegistry;
	class CMapsBaker;
	class CImageLoader;
}

namespace JDKLevelMaps::ViewModels
{
	enum class EOperationState : uint8
	{
		Idle,
		BakingMap,
		CancellingBake,
		LoadingPreview,
		CancellingPreview
	};

	class CLevelMapsViewModel : public QObject
	{
		Q_OBJECT
	public:
		CLevelMapsViewModel(QObject* pParent = nullptr);
		~CLevelMapsViewModel() noexcept;

		void RecomputePaths();
		void CheckPreviewAvailability();

		[[nodiscard]] EOperationState GetCurrentOperationState() const noexcept;
		[[nodiscard]] bool IsOperationCancelled() const noexcept;
		[[nodiscard]] Settings::SBakerSettings& GetSettings() noexcept;
		[[nodiscard]] const Settings::SBakerSettings& GetSettings() const noexcept;

		[[nodiscard]] float GetMaxCellSize() const noexcept;
		[[nodiscard]] uint32 CalculateMaxTileSize(float cellSize) const noexcept;

	public slots:
		void StartBaking();
		void StopBaking() noexcept;
		void LoadPreviewFromMapAsync();
		void LoadPreviewFromDiskAsync();
		void StopLoadingPreview() noexcept;
	
	signals:
		void progressUpdated(int percent);
		void operationStateChanged();
		void bakeFinished(bool bSuccess, QString message);
		void previewLoaded(QImage image);
		void previewLoadFailed(QString message);
		void previewAvailabilityChanged(bool bHasMap, bool bHasImage);

	private:
		[[nodiscard]] uint64 StartOperation(EOperationState state) noexcept;
		uint64 UpdateOperation(uint64 operationID, EOperationState state) noexcept;
		void ForceUpdateOperation(EOperationState state) noexcept;
		void StopProgressTimer();
		void JoinThread() noexcept;

	private:
		// @warning Not thread-safe.
		// All methods must be called from the main/UI thread.
		struct SOperationState
		{
			[[nodiscard]] EOperationState Get() const noexcept { return eCurrentState; }

			[[nodiscard]] uint64 Start(EOperationState state) noexcept
			{
				eCurrentState = state;
				return ++operationCounter;
			}

			[[nodiscard]] uint64 Update(uint64 operationID, EOperationState state) noexcept
			{
				if (operationID != operationCounter)
					return 0;

				return Start(state);
			}

			void ForceUpdate(EOperationState state) noexcept
			{
				eCurrentState = state;
			}

		private:
			EOperationState eCurrentState = EOperationState::Idle;
			uint64 operationCounter = 0;
		};

	private:
		std::unique_ptr<Settings::SBakerSettings>	m_pBakerSettings;
		std::unique_ptr<FileSystem::CPathResolver>	m_pPathResolver;
		std::unique_ptr<Managers::CBakersRegistry>	m_pBakersRegistry;
		std::unique_ptr<Managers::CMapsBaker>		m_pMapsBaker;
		std::unique_ptr<Managers::CImageLoader>		m_pImageLoader;

		Utils::Common::SProgress m_progress;
		SOperationState m_currentState;
		QTimer* m_pProgressTimer = nullptr;

		std::thread m_operationThread;
	};
}