#include "StdAfx.h"
#include "JDKLevelMapsEditor.h"

#include <QTabBar>
#include <QStackedWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QProgressBar>

#include "Editor/Components/MapPreview.h"
#include "Editor/Components/GenerateButton.h"
#include "Editor/ViewModels/LevelMapsViewModel.h"
#include "Editor/Views/VegetationEditor.h"

#include "Settings/BakerSettings.h"
#include "Utils/ConvertUtils.h"
#include "Utils/Logger.h"

namespace JDKLevelMaps::Views
{
	CJDKLevelMapsEditor::CJDKLevelMapsEditor(QWidget* pParent) : CDockableEditor(pParent)
	{
		m_pRootWidget = new QWidget();
		m_pViewModel = new ViewModels::CLevelMapsViewModel(this);

		LoadSettings();
		SetupWidget(m_pRootWidget);
		SetupConnections();
		SetContent(m_pRootWidget);

		UpdateLevelState(false);
		m_pViewModel->CheckPreviewAvailability(GetActiveMapType());
	}

	const char* CJDKLevelMapsEditor::GetEditorName() const noexcept { return "JDK Level Maps"; }

	void CJDKLevelMapsEditor::OnEditorNotifyEvent(EEditorNotifyEvent event)
	{
		switch (event)
		{
		case eNotify_OnBeginSceneClose:
		case eNotify_OnBeginNewScene:
		case eNotify_OnBeginLoad:
			UpdateLevelState(false);
			break;
		case eNotify_OnEndLoad:
			UpdateLevelState(true);
			break;
		}
	}

	void CJDKLevelMapsEditor::SetPluginProperty(const char* key, const QVariant& value)
	{
		SetProjectProperty(key, value);
	}

	QVariant CJDKLevelMapsEditor::GetPluginProperty(const char* key)
	{
		return GetProjectProperty(key);
	}

	void CJDKLevelMapsEditor::UpdateUIState()
	{
		const auto state = m_pViewModel->GetCurrentOperationState();
		const bool hasPreview = m_pMapPreview->HasPixmap();

		switch (state)
		{
		case ViewModels::EOperationState::Idle:
			m_pGenerateButton->setEnabled(m_bLevelLoaded);
			m_pGenerateButton->SetButtonState(Components::EButtonState::Start);
			m_pMapPreview->EnableLoadButton(true);
			m_pMapPreview->ShowLoadButton(Components::EButtonState::Start, !hasPreview && m_bHasMap);
			break;
		case ViewModels::EOperationState::BakingMap:
			m_pGenerateButton->setEnabled(true);
			m_pGenerateButton->SetButtonState(Components::EButtonState::Stop);
			m_pMapPreview->ShowLoadButton(false);
			break;
		case ViewModels::EOperationState::LoadingPreview:
			m_pGenerateButton->setEnabled(false);
			m_pMapPreview->ShowLoadButton(Components::EButtonState::Stop, true);
			break;
		case ViewModels::EOperationState::CancellingBake:
			m_pGenerateButton->setEnabled(false);
			m_pGenerateButton->SetButtonState(Components::EButtonState::Cancelling);
			m_pMapPreview->ShowLoadButton(false);
			break;
		case ViewModels::EOperationState::CancellingPreview:
			m_pGenerateButton->setEnabled(false);
			m_pMapPreview->EnableLoadButton(false);
			m_pMapPreview->ShowLoadButton(Components::EButtonState::Cancelling, true);
			break;
		}
	}

	void CJDKLevelMapsEditor::UpdateLevelState(bool bLevelLoaded)
	{
		m_bLevelLoaded = bLevelLoaded;
		m_pViewModel->RecomputePaths();
		m_pViewModel->CheckPreviewAvailability(GetActiveMapType());
		UpdateUIState();
	}

	void CJDKLevelMapsEditor::SetupWidget(QWidget* pWidget)
	{
		const auto& currentSettings = m_pViewModel->GetSettings();

		auto* pVegWidget = new CVegetationEditor(this, m_pStackedWidget);
		m_pViewModel->RegisterBaker(pVegWidget->CreateBaker());

		m_pStackedWidget = new QStackedWidget(pWidget);
		m_pStackedWidget->addWidget(pVegWidget);

		m_pTabBar = new QTabBar(pWidget);
		m_pTabBar->setDrawBase(false);
		m_pTabBar->setExpanding(false);
		m_pTabBar->setUsesScrollButtons(true);
		int tabIdx = m_pTabBar->addTab(tr("Vegetation"));
		m_pTabBar->setTabData(tabIdx, QVariant::fromValue(static_cast<int>(pVegWidget->GetMapType())));

		m_pMapPreview = new Components::CMapPreview(pWidget);

		m_pGenerateButton = new Components::CGenerateButton(pWidget);
		m_pGenerateButton->SetButtonState(Components::EButtonState::Start);
		m_pGenerateButton->SetText(Components::EButtonState::Start, tr("Generate"));
		m_pGenerateButton->SetText(Components::EButtonState::Stop, tr("Stop"));
		m_pGenerateButton->SetText(Components::EButtonState::Cancelling, tr("Cancelling..."));

		m_pFormatComboBox = new QComboBox(pWidget);
		m_pFormatComboBox->addItem("Bitmask", static_cast<int>(Settings::EDirectoryFormat::Bitmask));
		m_pFormatComboBox->addItem("Hybrid", static_cast<int>(Settings::EDirectoryFormat::Hybrid));
		int formatIndex = m_pFormatComboBox->findData(static_cast<int>(currentSettings.directoryFormat));
		m_pFormatComboBox->setCurrentIndex(formatIndex);

		m_pCellSizeSpinBox = new QDoubleSpinBox(pWidget);
		m_pCellSizeSpinBox->setDecimals(2);
		m_pCellSizeSpinBox->setSingleStep(0.5);
		m_pCellSizeSpinBox->setRange(0.1, m_pViewModel->GetMaxCellSize());
		m_pCellSizeSpinBox->setValue(currentSettings.cellSize);

		m_pTileSizeSpinBox = new QSpinBox(pWidget);
		m_pTileSizeSpinBox->setMinimum(1);
		m_pTileSizeSpinBox->setMaximum(m_pViewModel->CalculateMaxTileSize(currentSettings.cellSize));
		m_pTileSizeSpinBox->setValue(currentSettings.tileSize);

		m_pGenerateImageCheckBox = new QCheckBox(pWidget);
		m_pGenerateImageCheckBox->setChecked(currentSettings.bGenerateDebugImage);
		m_pGenerateImageCheckBox->setToolTip(tr("Generates a .png preview of the baked map on the disk.\n"
			"Warning: Exporting large maps may take a long time and consume a lot of RAM."));

		m_pProgressBar = new QProgressBar(pWidget);

		QFormLayout* pCommonForm = new QFormLayout();
		pCommonForm->addRow(tr("Directory Format"), m_pFormatComboBox);
		pCommonForm->addRow(tr("Cell Size"), m_pCellSizeSpinBox);
		pCommonForm->addRow(tr("Tile Size"), m_pTileSizeSpinBox);

		QHBoxLayout* pButtonLayout = new QHBoxLayout();
		pButtonLayout->setContentsMargins(0, 0, 0, 0);
		pButtonLayout->addWidget(m_pGenerateButton, 1);
		pButtonLayout->addWidget(m_pGenerateImageCheckBox, 0);

		QVBoxLayout* const pLayout = new QVBoxLayout(pWidget);
		pLayout->addWidget(m_pTabBar);
		pLayout->addWidget(m_pMapPreview, 1);
		pLayout->addLayout(pCommonForm);
		pLayout->addWidget(m_pStackedWidget);
		pLayout->addLayout(pButtonLayout);
		pLayout->addWidget(m_pProgressBar);
		pLayout->addStretch();
	}

	void CJDKLevelMapsEditor::SetupConnections()
	{
		auto& settings = m_pViewModel->GetSettings();

		connect(m_pTabBar, &QTabBar::currentChanged, m_pStackedWidget, &QStackedWidget::setCurrentIndex);

		connect(m_pTabBar, &QTabBar::currentChanged, this, [&]() {
			m_pViewModel->CheckPreviewAvailability(GetActiveMapType());
			});

		connect(m_pGenerateButton, &Components::CGenerateButton::buttonClicked, this, &CJDKLevelMapsEditor::OnGenerateButtonClicked);

		connect(m_pMapPreview, &Components::CMapPreview::loadPreviewClicked, this, &CJDKLevelMapsEditor::OnLoadPreviewButtonClicked);

		connect(m_pViewModel, &ViewModels::CLevelMapsViewModel::operationStateChanged, this, &CJDKLevelMapsEditor::OnOperationStateChanged);

		connect(m_pViewModel, &ViewModels::CLevelMapsViewModel::progressUpdated, m_pProgressBar, &QProgressBar::setValue);

		connect(m_pViewModel, &ViewModels::CLevelMapsViewModel::bakeFinished, this, &CJDKLevelMapsEditor::OnBakeFinished);

		connect(m_pViewModel, &ViewModels::CLevelMapsViewModel::previewLoadFailed, this, &CJDKLevelMapsEditor::OnPreviewLoadFailed);

		connect(m_pViewModel, &ViewModels::CLevelMapsViewModel::previewAvailabilityChanged, this, &CJDKLevelMapsEditor::OnPreviewAvailabilityChanged);

		connect(m_pViewModel, &ViewModels::CLevelMapsViewModel::previewLoaded, this, &CJDKLevelMapsEditor::OnPreviewLoaded);

		connect(m_pCellSizeSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &CJDKLevelMapsEditor::OnCellSizeChanged);

		connect(m_pGenerateImageCheckBox, &QCheckBox::toggled, this, [&](bool bChecked)
			{
				settings.bGenerateDebugImage = bChecked;
				SaveSettings();
			});

		connect(m_pFormatComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [&](int index) {
			settings.directoryFormat = static_cast<Settings::EDirectoryFormat>(m_pFormatComboBox->itemData(index).toInt());
			SaveSettings();
			});

		connect(m_pTileSizeSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [&](int value) {
			settings.tileSize = static_cast<uint32>(value);
			SaveSettings();
			});
	}

	void CJDKLevelMapsEditor::OnGenerateButtonClicked(Components::EButtonState clickedState)
	{
		if (clickedState == Components::EButtonState::Start)
		{
			m_pProgressBar->setValue(0);
			m_pViewModel->StartBaking(GetActiveMapType());
		}
		else
			m_pViewModel->StopBaking();
	}

	void CJDKLevelMapsEditor::OnLoadPreviewButtonClicked(bool bStart)
	{
		if (bStart)
		{
			m_pProgressBar->setValue(0);
			m_pViewModel->LoadPreviewFromMapAsync(GetActiveMapType());
		}
		else
			m_pViewModel->StopLoadingPreview();
	}

	void CJDKLevelMapsEditor::OnOperationStateChanged()
	{
		UpdateUIState();
	}

	void CJDKLevelMapsEditor::OnBakeFinished(bool bSuccess, QString message)
	{
		if (!bSuccess)
		{
			m_pProgressBar->setValue(0);

			if (!m_pViewModel->IsOperationCancelled())
				ShowError("Vegetation Level Map baking failed with error", message);
			else
				JDK_LOG("Level Map baking was cancelled by user");
		}
		else
		{
			m_pProgressBar->setValue(100);
			JDK_LOG("Vegetation Level Map has been baked successfully");
		}
	}

	void CJDKLevelMapsEditor::OnPreviewAvailabilityChanged(bool bHasMap, bool bHasImage)
	{
		if (!m_bLevelLoaded)
		{
			m_pMapPreview->ResetPixmap(tr("No level loaded"));
			return;
		}

		if (bHasImage)
		{
			m_pMapPreview->ResetPixmap(tr("Loading preview image..."));
			m_pViewModel->LoadPreviewFromDiskAsync(GetActiveMapType());
			return;
		}

		m_bHasMap = bHasMap;
		m_pMapPreview->ResetPixmap(tr("No preview generated yet"));
		UpdateUIState();
	}

	void CJDKLevelMapsEditor::OnPreviewLoaded(QImage image)
	{
		m_pProgressBar->setValue(100);
		m_pMapPreview->SetPixmap(QPixmap::fromImage(image));
	}

	void CJDKLevelMapsEditor::OnPreviewLoadFailed(QString message)
	{
		m_pProgressBar->setValue(0);

		if (!m_pViewModel->IsOperationCancelled())
			ShowError("Preview Image generation failed with error", message);
		else
			JDK_LOG("Preview loading was cancelled by user");

		m_pMapPreview->ResetPixmap(tr("No preview generated yet"));
	}

	void CJDKLevelMapsEditor::OnCellSizeChanged(double value)
	{
		auto& settings = m_pViewModel->GetSettings();
		settings.cellSize = static_cast<float>(value);

		m_pTileSizeSpinBox->setMaximum(m_pViewModel->CalculateMaxTileSize(settings.cellSize));
		SaveSettings();
	}

	void CJDKLevelMapsEditor::SaveSettings()
	{
		const auto& settings = m_pViewModel->GetSettings();

		SetProjectProperty("JDKLevelMaps/CellSize", settings.cellSize);
		SetProjectProperty("JDKLevelMaps/TileSize", settings.tileSize);
		SetProjectProperty("JDKLevelMaps/DirectoryFormat", static_cast<uint8>(settings.directoryFormat));
		SetProjectProperty("JDKLevelMaps/GenerateDebugImage", settings.bGenerateDebugImage);
	}

	void CJDKLevelMapsEditor::LoadSettings()
	{
		auto& settings = m_pViewModel->GetSettings();

		const float loadedCellSize = Utils::ConvertUtils::QVariantToFloat(GetProjectProperty("JDKLevelMaps/CellSize"), settings.cellSize);
		settings.cellSize = std::clamp(loadedCellSize, 0.1f, m_pViewModel->GetMaxCellSize());

		const uint32 loadedTileSize = Utils::ConvertUtils::QVariantToUint32(GetProjectProperty("JDKLevelMaps/TileSize"), settings.tileSize);
		settings.tileSize = std::clamp(loadedTileSize, static_cast<uint32>(1), m_pViewModel->CalculateMaxTileSize(settings.cellSize));

		const uint8 loadedFormat = Utils::ConvertUtils::QVariantToUint8(GetProjectProperty("JDKLevelMaps/DirectoryFormat"), static_cast<uint8>(settings.directoryFormat));
		settings.directoryFormat = static_cast<Settings::EDirectoryFormat>(std::clamp<uint8>(loadedFormat, 0, 1));

		settings.bGenerateDebugImage = Utils::ConvertUtils::QVariantToBool(GetProjectProperty("JDKLevelMaps/GenerateDebugImage"), settings.bGenerateDebugImage);
	}

	void CJDKLevelMapsEditor::ShowError(const QString& context, const QString& errorMsg)
	{
		JDK_ERR("%s: \"%s\"", qPrintable(context), qPrintable(errorMsg));
		QMessageBox::critical(this, tr("JDK Level Maps"), errorMsg);
	}

	EMapType CJDKLevelMapsEditor::GetActiveMapType() const noexcept
	{
		int mapTypeInt = m_pTabBar->tabData(m_pTabBar->currentIndex()).toInt();
		return static_cast<EMapType>(mapTypeInt);
	}
}

using CJDKLevelMapsEditor = JDKLevelMaps::Views::CJDKLevelMapsEditor;
REGISTER_VIEWPANE_FACTORY(CJDKLevelMapsEditor, "JDK Level Maps", "Tools", true);