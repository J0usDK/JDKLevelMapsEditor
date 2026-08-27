#include "StdAfx.h"
#include "LevelMapsViewModel.h"

#include <QTimer>
#include <CrySystem/File/ICryPak.h>

#include "Core/Data/RunResult.h"
#include "Core/Data/LevelContext.h"
#include "Core/BakersRegistry.h"
#include "Core/Orchestration/MapsBaker.h"
#include "Core/Orchestration/ImageLoader.h"
#include "Core/Bakers/Vegetation/VegetationBaker.h"
#include "Core/FileSystem/PathResolver.h"
#include "Settings/BakerSettings.h"
#include "Utils/ImageResizer.h"
#include "Utils/ImageView.h"

namespace JDKLevelMaps::ViewModels
{
	CLevelMapsViewModel::CLevelMapsViewModel(QObject* pParent) : QObject(pParent)
	{
		m_pBakerSettings = std::make_unique<Settings::SBakerSettings>();
		m_pPathResolver = std::make_unique<FileSystem::CPathResolver>();
		m_pBakersRegistry = std::make_unique<Managers::CBakersRegistry>();
		m_pMapsBaker = std::make_unique<Managers::CMapsBaker>(*m_pBakersRegistry.get(), *m_pPathResolver.get(), *m_pBakerSettings.get());
		m_pImageLoader = std::make_unique<Managers::CImageLoader>(*m_pBakersRegistry.get(), *m_pPathResolver.get());

		m_pBakersRegistry->RegisterBaker(std::make_unique<Bakers::CVegetationBaker>(m_pBakerSettings->vegSettings));

		m_pProgressTimer = new QTimer(this);
		m_pProgressTimer->setInterval(33);
		connect(m_pProgressTimer, &QTimer::timeout, this, [this]() { Q_EMIT progressUpdated(m_progress.progress.load(std::memory_order_relaxed)); });
	}

	CLevelMapsViewModel::~CLevelMapsViewModel() noexcept
	{
		if (m_operationThread.joinable())
			m_progress.bIsCancelled.store(true, std::memory_order_relaxed);

		JoinThread();
	}

	void CLevelMapsViewModel::RecomputePaths()
	{
		m_pPathResolver->RecomputePath();
		CheckPreviewAvailability();
	}

	void CLevelMapsViewModel::CheckPreviewAvailability()
	{
		auto pBaker = m_pBakersRegistry->GetBaker(EMapType::VegetationDensity);
		if (!pBaker) return;

		auto imagePathOpt = m_pPathResolver->GetImagePath(pBaker->GetID());
		auto mapPathOpt = m_pPathResolver->GetMapPath(pBaker->GetID());

		const bool bHasImage = imagePathOpt && gEnv->pCryPak->IsFileExist(imagePathOpt->c_str());
		const bool bHasMap = mapPathOpt && gEnv->pCryPak->IsFileExist(mapPathOpt->c_str());

		Q_EMIT previewAvailabilityChanged(bHasMap, bHasImage, bHasImage ? QString::fromStdString(imagePathOpt.value()) : QString());
	}

	bool CLevelMapsViewModel::IsOperationCancelled() const noexcept
	{
		return m_progress.bIsCancelled.load(std::memory_order_relaxed);
	}

	void CLevelMapsViewModel::StartBaking()
	{
		JoinThread();
		m_progress.Reset();
		const uint64 operationID = StartOperation(EOperationState::BakingMap);

		m_pProgressTimer->start();
		m_operationThread = std::thread([this, operationID]()
		{
			Data::SRunResult result = m_pMapsBaker->RunBake(EMapType::VegetationDensity, m_progress);

			QMetaObject::invokeMethod(this, [this, operationID, operationResult = std::move(result)]()
			{
				StopProgressTimer();
				Q_EMIT progressUpdated(100);
				Q_EMIT bakeFinished(operationResult.bSuccess, QString::fromStdString(operationResult.message));
				if (UpdateOperation(operationID, EOperationState::Idle) != 0)
					CheckPreviewAvailability();
			});
		});
	}

	void CLevelMapsViewModel::LoadPreviewAsync()
	{
		JoinThread();
		m_progress.Reset();
		const uint64 operationID = StartOperation(EOperationState::LoadingPreview);

		m_pProgressTimer->start();
		m_operationThread = std::thread([this, operationID]()
		{
			QImage img(0, 0, QImage::Format_RGB888);

			ImageWork::SImageView imageView(0, 0, 0, 3, img.bits(), & Utils::Image::ResizeImage, &img);
			Data::SRunResult result = m_pImageLoader->LoadPreviewFromMap(EMapType::VegetationDensity, m_progress, imageView);

			QMetaObject::invokeMethod(this, [this, operationID, img, operationResult = std::move(result)]()
			{
				StopProgressTimer();
				Q_EMIT progressUpdated(100);
				if (operationResult.bSuccess)
					Q_EMIT previewLoaded(img);
				else
					Q_EMIT previewLoadFailed(QString::fromStdString(operationResult.message));

				UpdateOperation(operationID, EOperationState::Idle);
			});
		});
	}

	void CLevelMapsViewModel::LoadPreviewAsync(const QString& imagePath)
	{
		JoinThread();
		m_progress.Reset();
		const uint64 operationID = StartOperation(EOperationState::LoadingPreview);

		m_operationThread = std::thread([this, operationID, path = imagePath]()
		{
			QImage img(0, 0, QImage::Format_RGB888);

			ImageWork::SImageView imageView(0, 0, 0, 3, img.bits(), &Utils::Image::ResizeImage, &img);
			Data::SRunResult result = m_pImageLoader->LoadPreviewFromMap(EMapType::VegetationDensity, m_progress, imageView);

			m_progress.bIsCompleted.store(true, std::memory_order_release);
			QMetaObject::invokeMethod(this, [this, operationID, img, operationResult = std::move(result)]()
			{
				if (operationResult.bSuccess)
					Q_EMIT previewLoaded(img);
				else
					Q_EMIT previewLoadFailed(QString::fromStdString(operationResult.message));

				UpdateOperation(operationID, EOperationState::Idle);
			});
		});
	}

	void CLevelMapsViewModel::StopBaking() noexcept
	{
		if (!m_operationThread.joinable())
			return;

		m_progress.bIsCancelled.store(true, std::memory_order_relaxed);
		ForceUpdateOperation(EOperationState::CancellingBake);
	}

	void CLevelMapsViewModel::StopLoadingPreview() noexcept
	{
		if (!m_operationThread.joinable())
			return;

		m_progress.bIsCancelled.store(true, std::memory_order_relaxed);
		ForceUpdateOperation(EOperationState::CancellingPreview);
	}

	float CLevelMapsViewModel::GetMaxCellSize() const noexcept
	{
		const int terrainSize = Data::GetLevelTerrainSize();
		CRY_ASSERT(terrainSize > 0, "[JDKLevelMaps] Invalid terrain size");

		return static_cast<float>(terrainSize);
	}

	uint32 CLevelMapsViewModel::CalculateMaxTileSize(float cellSize) const noexcept
	{
		CRY_ASSERT(std::isfinite(cellSize) && cellSize > 0.0f);
		const int terrainSize = Data::GetLevelTerrainSize();
		CRY_ASSERT(terrainSize > 0, "[JDKLevelMaps] Invalid terrain size");

		return std::max(1u, static_cast<uint32>(std::round(static_cast<double>(terrainSize) / static_cast<double>(cellSize))));
	}

	uint64 CLevelMapsViewModel::StartOperation(EOperationState state) noexcept
	{
		uint64 nextID = m_currentState.Start(state);
		Q_EMIT operationStateChanged();
		return nextID;
	}

	uint64 CLevelMapsViewModel::UpdateOperation(uint64 operationID, EOperationState state) noexcept
	{
		uint64 nextID = m_currentState.Update(operationID, state);
		if (nextID != 0)
			Q_EMIT operationStateChanged();
		return nextID;
	}

	void CLevelMapsViewModel::ForceUpdateOperation(EOperationState state) noexcept
	{
		m_currentState.ForceUpdate(state);
		Q_EMIT operationStateChanged();
	}

	void CLevelMapsViewModel::StopProgressTimer()
	{
		m_pProgressTimer->stop();
	}

	void CLevelMapsViewModel::JoinThread() noexcept
	{
		if (m_operationThread.joinable())
			m_operationThread.join();
	}

	EOperationState CLevelMapsViewModel::GetCurrentOperationState() const noexcept { return m_currentState.Get(); }
	Settings::SBakerSettings& CLevelMapsViewModel::GetSettings() noexcept { return *m_pBakerSettings; }
	const Settings::SBakerSettings& CLevelMapsViewModel::GetSettings() const noexcept { return *m_pBakerSettings; }
}