#include "StdAfx.h"
#include "JDKLevelMapsEditor.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QProgressBar>

#include "Components/MapPreview.h"
#include "Components/GenerateButton.h"
#include "ViewModels/LevelMapsViewModel.h"

#include "Settings/BakerSettings.h"
#include "Utils/ConvertUtils.h"
#include "Utils/Logger.h"

CJDKLevelMapsEditor::CJDKLevelMapsEditor(QWidget* pParent) : CDockableEditor(pParent)
{
	m_pRootWidget = new QWidget();
	m_pViewModel = new JDKLevelMaps::ViewModels::CLevelMapsViewModel(this);

	LoadSettings();
	SetupWidget(m_pRootWidget);
	SetupConnections();
	SetContent(m_pRootWidget);

	UpdateLevelState(false);
	m_pViewModel->CheckPreviewAvailability();
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

void CJDKLevelMapsEditor::UpdateUIState()
{
	const auto state = m_pViewModel->GetCurrentOperationState();
	const bool hasPreview = m_pMapPreview->HasPixmap();

	switch (state)
	{
	case JDKLevelMaps::ViewModels::EOperationState::Idle:
		m_pGenerateButton->setEnabled(m_bLevelLoaded);
		m_pGenerateButton->SetButtonState(JDKLevelMaps::Components::EButtonState::Start);
		m_pMapPreview->EnableLoadButton(true);
		m_pMapPreview->ShowLoadButton(JDKLevelMaps::Components::EButtonState::Start, !hasPreview && m_bHasMap);
		break;
	case JDKLevelMaps::ViewModels::EOperationState::BakingMap:
		m_pGenerateButton->setEnabled(true);
		m_pGenerateButton->SetButtonState(JDKLevelMaps::Components::EButtonState::Stop);
		m_pMapPreview->ShowLoadButton(false);
		break;
	case JDKLevelMaps::ViewModels::EOperationState::LoadingPreview:
		m_pGenerateButton->setEnabled(false);
		m_pMapPreview->ShowLoadButton(JDKLevelMaps::Components::EButtonState::Stop, true);
		break;
	case JDKLevelMaps::ViewModels::EOperationState::CancellingBake:
		m_pGenerateButton->setEnabled(false);
		m_pGenerateButton->SetButtonState(JDKLevelMaps::Components::EButtonState::Cancelling);
		m_pMapPreview->ShowLoadButton(false);
		break;
	case JDKLevelMaps::ViewModels::EOperationState::CancellingPreview:
		m_pGenerateButton->setEnabled(false);
		m_pMapPreview->EnableLoadButton(false);
		m_pMapPreview->ShowLoadButton(JDKLevelMaps::Components::EButtonState::Cancelling, true);
		break;
	}
}

void CJDKLevelMapsEditor::UpdateLevelState(bool bLevelLoaded)
{
	m_bLevelLoaded = bLevelLoaded;
	m_pViewModel->RecomputePaths();
	UpdateUIState();
}

void CJDKLevelMapsEditor::SetupWidget(QWidget* pWidget)
{
	m_pMapPreview = new JDKLevelMaps::Components::CMapPreview(pWidget);
	m_pGenerateButton = new JDKLevelMaps::Components::CGenerateButton(pWidget);
	m_pFormatComboBox = new QComboBox(pWidget);
	m_pCellSizeSpinBox = new QDoubleSpinBox(pWidget);
	m_pTileSizeSpinBox = new QSpinBox(pWidget);
	m_pSensitivitySpinBox = new QSpinBox(pWidget);
	m_pGrassCheckBox = new QCheckBox(pWidget);
	m_pBushCheckBox = new QCheckBox(pWidget);
	m_pTreeCheckBox = new QCheckBox(pWidget);
	m_pGrassLineEdit = new QLineEdit(pWidget);
	m_pBushLineEdit = new QLineEdit(pWidget);
	m_pTreeLineEdit = new QLineEdit(pWidget);
	m_pGenerateImageCheckBox = new QCheckBox(pWidget);
	m_pProgressBar = new QProgressBar(pWidget);

	const auto& currentSettings = m_pViewModel->GetSettings();

	m_pGenerateButton->SetButtonState(JDKLevelMaps::Components::EButtonState::Start);
	m_pGenerateButton->SetText(JDKLevelMaps::Components::EButtonState::Start, tr("Generate"));
	m_pGenerateButton->SetText(JDKLevelMaps::Components::EButtonState::Stop, tr("Stop"));
	m_pGenerateButton->SetText(JDKLevelMaps::Components::EButtonState::Cancelling, tr("Cancelling..."));

	m_pFormatComboBox->addItem("Bitmask", static_cast<int>(JDKLevelMaps::Settings::EDirectoryFormat::Bitmask));
	m_pFormatComboBox->addItem("Hybrid", static_cast<int>(JDKLevelMaps::Settings::EDirectoryFormat::Hybrid));
	
	int formatIndex = m_pFormatComboBox->findData(static_cast<int>(currentSettings.directoryFormat));
	m_pFormatComboBox->setCurrentIndex(formatIndex);

	m_pCellSizeSpinBox->setDecimals(2);
	m_pCellSizeSpinBox->setSingleStep(0.5);
	m_pCellSizeSpinBox->setRange(0.1, m_pViewModel->GetMaxCellSize());
	m_pCellSizeSpinBox->setValue(currentSettings.cellSize);

	m_pTileSizeSpinBox->setMinimum(1);
	m_pTileSizeSpinBox->setMaximum(m_pViewModel->CalculateMaxTileSize(currentSettings.cellSize));
	m_pTileSizeSpinBox->setValue(currentSettings.tileSize);

	m_pSensitivitySpinBox->setMinimum(0);
	m_pSensitivitySpinBox->setMaximum(255);
	m_pSensitivitySpinBox->setValue(currentSettings.vegSettings.densityPerInstance);

	m_pGrassCheckBox->setChecked(currentSettings.vegSettings.bEnableGrass);
	m_pBushCheckBox->setChecked(currentSettings.vegSettings.bEnableBush);
	m_pTreeCheckBox->setChecked(currentSettings.vegSettings.bEnableTree);

	m_pGrassLineEdit->setText(QString::fromStdString(currentSettings.vegSettings.grassGroupName));
	m_pBushLineEdit->setText(QString::fromStdString(currentSettings.vegSettings.bushGroupName));
	m_pTreeLineEdit->setText(QString::fromStdString(currentSettings.vegSettings.treeGroupName));

	m_pGenerateImageCheckBox->setChecked(currentSettings.bGenerateDebugImage);
	m_pGenerateImageCheckBox->setToolTip(tr("Generates a .png preview of the baked map on the disk.\n"
		"Warning: Exporting large maps may take a long time and consume a lot of RAM."));

	QFormLayout* pForm = new QFormLayout();
	pForm->addRow(tr("Directory Format"), m_pFormatComboBox);
	pForm->addRow(tr("Cell Size"), m_pCellSizeSpinBox);
	pForm->addRow(tr("Tile Size"), m_pTileSizeSpinBox);
	pForm->addRow(tr("Sensitivity"), m_pSensitivitySpinBox);
	pForm->addRow(tr("Enable Grass Layer"), m_pGrassCheckBox);
	pForm->addRow(tr("Enable Bush Layer"), m_pBushCheckBox);
	pForm->addRow(tr("Enable Tree Layer"), m_pTreeCheckBox);
	pForm->addRow(tr("Grass Group Name"), m_pGrassLineEdit);
	pForm->addRow(tr("Bush Group Name"), m_pBushLineEdit);
	pForm->addRow(tr("Tree Group Name"), m_pTreeLineEdit);

	QHBoxLayout* pButtonLayout = new QHBoxLayout();
	pButtonLayout->setContentsMargins(0, 0, 0, 0);
	pButtonLayout->addWidget(m_pGenerateButton, 1);
	pButtonLayout->addWidget(m_pGenerateImageCheckBox, 0);

	QVBoxLayout* const pLayout = new QVBoxLayout(pWidget);
	pLayout->addWidget(m_pMapPreview, 1);
	pLayout->addLayout(pForm);
	pLayout->addLayout(pButtonLayout);
	pLayout->addWidget(m_pProgressBar);
	pLayout->addStretch();
}

void CJDKLevelMapsEditor::SetupConnections()
{
	auto& settings = m_pViewModel->GetSettings();

	connect(m_pGenerateButton, &JDKLevelMaps::Components::CGenerateButton::buttonClicked, this, &CJDKLevelMapsEditor::OnGenerateButtonClicked);

	connect(m_pMapPreview, &JDKLevelMaps::Components::CMapPreview::loadPreviewClicked, this, &CJDKLevelMapsEditor::OnLoadPreviewButtonClicked);

	connect(m_pViewModel, &JDKLevelMaps::ViewModels::CLevelMapsViewModel::operationStateChanged, this, &CJDKLevelMapsEditor::OnOperationStateChanged);
	
	connect(m_pViewModel, &JDKLevelMaps::ViewModels::CLevelMapsViewModel::progressUpdated, m_pProgressBar, &QProgressBar::setValue);

	connect(m_pViewModel, &JDKLevelMaps::ViewModels::CLevelMapsViewModel::bakeFinished, this, &CJDKLevelMapsEditor::OnBakeFinished);

	connect(m_pViewModel, &JDKLevelMaps::ViewModels::CLevelMapsViewModel::previewLoadFailed, this, &CJDKLevelMapsEditor::OnPreviewLoadFailed);

	connect(m_pViewModel, &JDKLevelMaps::ViewModels::CLevelMapsViewModel::previewAvailabilityChanged, this, &CJDKLevelMapsEditor::OnPreviewAvailabilityChanged);

	connect(m_pViewModel, &JDKLevelMaps::ViewModels::CLevelMapsViewModel::previewLoaded, this, &CJDKLevelMapsEditor::OnPreviewLoaded);

	connect(m_pCellSizeSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &CJDKLevelMapsEditor::OnCellSizeChanged);

	connect(m_pGenerateImageCheckBox, &QCheckBox::toggled, this, [&](bool bChecked)
	{
		settings.bGenerateDebugImage = bChecked;
		SaveSettings();
	});
	
	connect(m_pFormatComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [&](int index) {
		settings.directoryFormat = static_cast<JDKLevelMaps::Settings::EDirectoryFormat>(m_pFormatComboBox->itemData(index).toInt());
		SaveSettings();
	});

	connect(m_pTileSizeSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [&](int value) {
		settings.tileSize = static_cast<uint32>(value);
		SaveSettings();
	});

	connect(m_pSensitivitySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [&](int value) {
		settings.vegSettings.densityPerInstance = static_cast<uint8>(value);
		SaveVegetationSettings(settings.vegSettings);
	});

	connect(m_pGrassCheckBox, &QCheckBox::toggled, this, [&](bool bChecked) {
		settings.vegSettings.bEnableGrass = bChecked;
		SaveVegetationSettings(settings.vegSettings);
	});

	connect(m_pBushCheckBox, &QCheckBox::toggled, this, [&](bool bChecked) {
		settings.vegSettings.bEnableBush = bChecked;
		SaveVegetationSettings(settings.vegSettings);
	});

	connect(m_pTreeCheckBox, &QCheckBox::toggled, this, [&](bool bChecked) {
		settings.vegSettings.bEnableTree = bChecked;
		SaveVegetationSettings(settings.vegSettings);
	});

	connect(m_pGrassLineEdit, &QLineEdit::editingFinished, this, [&]() {
		settings.vegSettings.grassGroupName = m_pGrassLineEdit->text().toStdString();
		SaveVegetationSettings(settings.vegSettings);
	});

	connect(m_pBushLineEdit, &QLineEdit::editingFinished, this, [&]() {
		settings.vegSettings.bushGroupName = m_pBushLineEdit->text().toStdString();
		SaveVegetationSettings(settings.vegSettings);
	});

	connect(m_pTreeLineEdit, &QLineEdit::editingFinished, this, [&]() {
		settings.vegSettings.treeGroupName = m_pTreeLineEdit->text().toStdString();
		SaveVegetationSettings(settings.vegSettings);
	});
}

void CJDKLevelMapsEditor::OnGenerateButtonClicked(JDKLevelMaps::Components::EButtonState clickedState)
{
	if (clickedState == JDKLevelMaps::Components::EButtonState::Start)
	{
		m_pProgressBar->setValue(0);
		m_pViewModel->StartBaking();
	}
	else
		m_pViewModel->StopBaking();
}

void CJDKLevelMapsEditor::OnLoadPreviewButtonClicked(bool bStart)
{
	if (bStart)
	{
		m_pProgressBar->setValue(0);
		m_pViewModel->LoadPreviewFromMapAsync();
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
		m_pViewModel->LoadPreviewFromDiskAsync();
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
	SaveVegetationSettings(settings.vegSettings);
}

void CJDKLevelMapsEditor::SaveVegetationSettings(const JDKLevelMaps::Settings::SVegetationBakerSettings& vegSettings)
{
	SetProjectProperty("JDKLevelMaps/DensityPerInstance", vegSettings.densityPerInstance);
	SetProjectProperty("JDKLevelMaps/GrassGroupName", QString::fromStdString(vegSettings.grassGroupName));
	SetProjectProperty("JDKLevelMaps/BushGroupName", QString::fromStdString(vegSettings.bushGroupName));
	SetProjectProperty("JDKLevelMaps/TreeGroupName", QString::fromStdString(vegSettings.treeGroupName));
	SetProjectProperty("JDKLevelMaps/EnableGrass", vegSettings.bEnableGrass);
	SetProjectProperty("JDKLevelMaps/EnableBush", vegSettings.bEnableBush);
	SetProjectProperty("JDKLevelMaps/EnableTree", vegSettings.bEnableTree);
}

void CJDKLevelMapsEditor::LoadSettings()
{
	auto& settings = m_pViewModel->GetSettings();

	const float loadedCellSize = JDKLevelMaps::Utils::ConvertUtils::QVariantToFloat(GetProjectProperty("JDKLevelMaps/CellSize"), settings.cellSize);
	settings.cellSize = std::clamp(loadedCellSize, 0.1f, m_pViewModel->GetMaxCellSize());

	const uint32 loadedTileSize = JDKLevelMaps::Utils::ConvertUtils::QVariantToUint32(GetProjectProperty("JDKLevelMaps/TileSize"), settings.tileSize);
	settings.tileSize = std::clamp(loadedTileSize, static_cast<uint32>(1), m_pViewModel->CalculateMaxTileSize(settings.cellSize));

	const uint8 loadedFormat = JDKLevelMaps::Utils::ConvertUtils::QVariantToUint8(GetProjectProperty("JDKLevelMaps/DirectoryFormat"), static_cast<uint8>(settings.directoryFormat));
	settings.directoryFormat = static_cast<JDKLevelMaps::Settings::EDirectoryFormat>(std::clamp<uint8>(loadedFormat, 0, 1));

	settings.bGenerateDebugImage = JDKLevelMaps::Utils::ConvertUtils::QVariantToBool(GetProjectProperty("JDKLevelMaps/GenerateDebugImage"), settings.bGenerateDebugImage);

	LoadVegetationSettings(settings.vegSettings);
}

void CJDKLevelMapsEditor::LoadVegetationSettings(JDKLevelMaps::Settings::SVegetationBakerSettings& vegSettings)
{
	vegSettings.densityPerInstance = JDKLevelMaps::Utils::ConvertUtils::QVariantToUint8(GetProjectProperty("JDKLevelMaps/DensityPerInstance"), vegSettings.densityPerInstance);
	vegSettings.grassGroupName = JDKLevelMaps::Utils::ConvertUtils::QVariantToStdString(GetProjectProperty("JDKLevelMaps/GrassGroupName"), vegSettings.grassGroupName);
	vegSettings.bushGroupName = JDKLevelMaps::Utils::ConvertUtils::QVariantToStdString(GetProjectProperty("JDKLevelMaps/BushGroupName"), vegSettings.bushGroupName);
	vegSettings.treeGroupName = JDKLevelMaps::Utils::ConvertUtils::QVariantToStdString(GetProjectProperty("JDKLevelMaps/TreeGroupName"), vegSettings.treeGroupName);
	vegSettings.bEnableGrass = JDKLevelMaps::Utils::ConvertUtils::QVariantToBool(GetProjectProperty("JDKLevelMaps/EnableGrass"), vegSettings.bEnableGrass);
	vegSettings.bEnableBush = JDKLevelMaps::Utils::ConvertUtils::QVariantToBool(GetProjectProperty("JDKLevelMaps/EnableBush"), vegSettings.bEnableBush);
	vegSettings.bEnableTree = JDKLevelMaps::Utils::ConvertUtils::QVariantToBool(GetProjectProperty("JDKLevelMaps/EnableTree"), vegSettings.bEnableTree);
}

void CJDKLevelMapsEditor::ShowError(const QString& context, const QString& errorMsg)
{
	JDK_ERR("%s: \"%s\"", qPrintable(context), qPrintable(errorMsg));
	QMessageBox::critical(this, tr("JDK Level Maps"), errorMsg);
}

REGISTER_VIEWPANE_FACTORY(CJDKLevelMapsEditor, "JDK Level Maps", "Tools", true);