#include "StdAfx.h"
#include "VegetationEditor.h"

#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QFormLayout>

#include "Core/Bakers/Vegetation/VegetationBaker.h"
#include "Settings/ISettingsManager.h"
#include "Utils/ConvertUtils.h"
#include "Shared/MapHeader.h"

namespace JDKLevelMaps::Views
{
	CVegetationEditor::CVegetationEditor(Settings::ISettingsManager* pSettingsManager, QWidget* pParent)
		: QWidget(pParent), m_pSettingsManager(pSettingsManager)
	{
		LoadSettings();
		SetupUI();
		SetupConnections();
	}

	std::unique_ptr<Bakers::IMapBaker> CVegetationEditor::CreateBaker() const noexcept
	{
		return std::make_unique<Bakers::CVegetationBaker>(m_settings);
	}

	EMapType CVegetationEditor::GetMapType() const noexcept
	{
		return EMapType::VegetationDensity;
	}

	void CVegetationEditor::SetupUI()
	{
		m_pSensitivitySpinBox = new QSpinBox(this);
		m_pSensitivitySpinBox->setMinimum(0);
		m_pSensitivitySpinBox->setMaximum(255);
		m_pSensitivitySpinBox->setValue(m_settings.densityPerInstance);

		m_pSimulateSizeCheckBox = new QCheckBox(this);
		m_pSimulateSizeCheckBox->setChecked(m_settings.bSimulateSize);

		m_pGrassCheckBox = new QCheckBox(this);
		m_pGrassCheckBox->setChecked(m_settings.bEnableGrass);

		m_pBushCheckBox = new QCheckBox(this);
		m_pBushCheckBox->setChecked(m_settings.bEnableBush);

		m_pTreeCheckBox = new QCheckBox(this);
		m_pTreeCheckBox->setChecked(m_settings.bEnableTree);

		m_pSizeMethodComboBox = new QComboBox(this);
		m_pSizeMethodComboBox->addItem(tr("Base"), static_cast<int>(Settings::ESimulateSizeMethod::Base));
		m_pSizeMethodComboBox->addItem(tr("Canopy"), static_cast<int>(Settings::ESimulateSizeMethod::Canopy));
		int sizeMethodIndex = m_pSizeMethodComboBox->findData(static_cast<int>(m_settings.sizeMethod));
		m_pSizeMethodComboBox->setCurrentIndex(sizeMethodIndex);
		m_pSizeMethodComboBox->setEnabled(m_settings.bSimulateSize);

		m_pGrassLineEdit = new QLineEdit(this);
		m_pGrassLineEdit->setText(QString::fromStdString(m_settings.grassGroupName));
		m_pGrassLineEdit->setEnabled(m_settings.bEnableGrass);

		m_pBushLineEdit = new QLineEdit(this);
		m_pBushLineEdit->setText(QString::fromStdString(m_settings.bushGroupName));
		m_pBushLineEdit->setEnabled(m_settings.bEnableBush);

		m_pTreeLineEdit = new QLineEdit(this);
		m_pTreeLineEdit->setText(QString::fromStdString(m_settings.treeGroupName));
		m_pTreeLineEdit->setEnabled(m_settings.bEnableTree);

		QHBoxLayout* pSimulateSizeLayout = new QHBoxLayout();
		pSimulateSizeLayout->setContentsMargins(0, 0, 0, 0);
		pSimulateSizeLayout->addWidget(m_pSimulateSizeCheckBox, 0);
		pSimulateSizeLayout->addWidget(m_pSizeMethodComboBox, 1);

		QHBoxLayout* pGrassLayerLayout = new QHBoxLayout();
		pGrassLayerLayout->setContentsMargins(0, 0, 0, 0);
		pGrassLayerLayout->addWidget(m_pGrassCheckBox, 0);
		pGrassLayerLayout->addWidget(m_pGrassLineEdit, 1);

		QHBoxLayout* pBushLayerLayout = new QHBoxLayout();
		pBushLayerLayout->setContentsMargins(0, 0, 0, 0);
		pBushLayerLayout->addWidget(m_pBushCheckBox, 0);
		pBushLayerLayout->addWidget(m_pBushLineEdit, 1);

		QHBoxLayout* pTreeLayerLayout = new QHBoxLayout();
		pTreeLayerLayout->setContentsMargins(0, 0, 0, 0);
		pTreeLayerLayout->addWidget(m_pTreeCheckBox, 0);
		pTreeLayerLayout->addWidget(m_pTreeLineEdit, 1);

		QFormLayout* pForm = new QFormLayout(this);
		pForm->setContentsMargins(0, 0, 0, 0);
		pForm->addRow(tr("Sensitivity"), m_pSensitivitySpinBox);
		pForm->addRow(tr("Simulate Size"), pSimulateSizeLayout);
		pForm->addRow(tr("Grass Layer"), pGrassLayerLayout);
		pForm->addRow(tr("Bush Layer"), pBushLayerLayout);
		pForm->addRow(tr("Tree Layer"), pTreeLayerLayout);
	}

	void CVegetationEditor::SetupConnections()
	{
		connect(m_pSensitivitySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [&](int value) {
			m_settings.densityPerInstance = static_cast<uint8>(value);
			SaveSettings();
		});

		connect(m_pSimulateSizeCheckBox, &QCheckBox::toggled, this, [&](bool bChecked) {
			m_settings.bSimulateSize = bChecked;
			m_pSizeMethodComboBox->setEnabled(bChecked);
			SaveSettings();
		});

		connect(m_pGrassCheckBox, &QCheckBox::toggled, this, [&](bool bChecked) {
			m_settings.bEnableGrass = bChecked;
			m_pGrassLineEdit->setEnabled(bChecked);
			SaveSettings();
		});

		connect(m_pBushCheckBox, &QCheckBox::toggled, this, [&](bool bChecked) {
			m_settings.bEnableBush = bChecked;
			m_pBushLineEdit->setEnabled(bChecked);
			SaveSettings();
		});

		connect(m_pTreeCheckBox, &QCheckBox::toggled, this, [&](bool bChecked) {
			m_settings.bEnableTree = bChecked;
			m_pTreeLineEdit->setEnabled(bChecked);
			SaveSettings();
		});

		connect(m_pSizeMethodComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
			m_settings.sizeMethod = static_cast<Settings::ESimulateSizeMethod>(m_pSizeMethodComboBox->itemData(index).toInt());
			SaveSettings();
		});

		connect(m_pGrassLineEdit, &QLineEdit::editingFinished, this, [&]() {
			m_settings.grassGroupName = m_pGrassLineEdit->text().toStdString();
			SaveSettings();
		});

		connect(m_pBushLineEdit, &QLineEdit::editingFinished, this, [&]() {
			m_settings.bushGroupName = m_pBushLineEdit->text().toStdString();
			SaveSettings();
		});

		connect(m_pTreeLineEdit, &QLineEdit::editingFinished, this, [&]() {
			m_settings.treeGroupName = m_pTreeLineEdit->text().toStdString();
			SaveSettings();
		});
	}

	void CVegetationEditor::LoadSettings()
	{
		m_settings.densityPerInstance = Utils::ConvertUtils::QVariantToUint8(m_pSettingsManager->GetPluginProperty("JDKLevelMaps/DensityPerInstance"), m_settings.densityPerInstance);
		m_settings.bSimulateSize = Utils::ConvertUtils::QVariantToBool(m_pSettingsManager->GetPluginProperty("JDKLevelMaps/SimulateSize"), m_settings.bSimulateSize);
		m_settings.bEnableGrass = Utils::ConvertUtils::QVariantToBool(m_pSettingsManager->GetPluginProperty("JDKLevelMaps/EnableGrass"), m_settings.bEnableGrass);
		m_settings.bEnableBush = Utils::ConvertUtils::QVariantToBool(m_pSettingsManager->GetPluginProperty("JDKLevelMaps/EnableBush"), m_settings.bEnableBush);
		m_settings.bEnableTree = Utils::ConvertUtils::QVariantToBool(m_pSettingsManager->GetPluginProperty("JDKLevelMaps/EnableTree"), m_settings.bEnableTree);
		
		uint8 sizeMethod = Utils::ConvertUtils::QVariantToUint8(m_pSettingsManager->GetPluginProperty("JDKLevelMaps/SimulateSizeMethod"), static_cast<uint8>(m_settings.sizeMethod));
		m_settings.sizeMethod = static_cast<Settings::ESimulateSizeMethod>(std::clamp<uint8>(sizeMethod, 0, static_cast<uint8>(Settings::ESimulateSizeMethod::Canopy)));
		
		m_settings.grassGroupName = Utils::ConvertUtils::QVariantToStdString(m_pSettingsManager->GetPluginProperty("JDKLevelMaps/GrassGroupName"), m_settings.grassGroupName);
		m_settings.bushGroupName = Utils::ConvertUtils::QVariantToStdString(m_pSettingsManager->GetPluginProperty("JDKLevelMaps/BushGroupName"), m_settings.bushGroupName);
		m_settings.treeGroupName = Utils::ConvertUtils::QVariantToStdString(m_pSettingsManager->GetPluginProperty("JDKLevelMaps/TreeGroupName"), m_settings.treeGroupName);
	}

	void CVegetationEditor::SaveSettings()
	{
		m_pSettingsManager->SetPluginProperty("JDKLevelMaps/DensityPerInstance", m_settings.densityPerInstance);
		m_pSettingsManager->SetPluginProperty("JDKLevelMaps/SimulateSize", m_settings.bSimulateSize);
		m_pSettingsManager->SetPluginProperty("JDKLevelMaps/EnableGrass", m_settings.bEnableGrass);
		m_pSettingsManager->SetPluginProperty("JDKLevelMaps/EnableBush", m_settings.bEnableBush);
		m_pSettingsManager->SetPluginProperty("JDKLevelMaps/EnableTree", m_settings.bEnableTree);
		m_pSettingsManager->SetPluginProperty("JDKLevelMaps/SimulateSizeMethod", static_cast<uint8>(m_settings.sizeMethod));
		m_pSettingsManager->SetPluginProperty("JDKLevelMaps/GrassGroupName", QString::fromStdString(m_settings.grassGroupName));
		m_pSettingsManager->SetPluginProperty("JDKLevelMaps/BushGroupName", QString::fromStdString(m_settings.bushGroupName));
		m_pSettingsManager->SetPluginProperty("JDKLevelMaps/TreeGroupName", QString::fromStdString(m_settings.treeGroupName));
	}
}