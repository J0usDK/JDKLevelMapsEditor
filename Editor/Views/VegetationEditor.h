#pragma once
#include <memory>
#include <QWidget>

#include <CryCore/BaseTypes.h>

#include "Settings/VegetationBakerSettings.h"

class QSpinBox;
class QCheckBox;
class QLineEdit;

namespace JDKLevelMaps
{
	enum class EMapType : uint8;
}

namespace JDKLevelMaps::Bakers
{
	class IMapBaker;
}

namespace JDKLevelMaps::Settings
{
	class ISettingsManager;
}

namespace JDKLevelMaps::Views
{
	class CVegetationEditor : public QWidget
	{
		Q_OBJECT
	public:
		explicit CVegetationEditor(Settings::ISettingsManager* pSettingsManager, QWidget* pParent = nullptr);

		[[nodiscard]] std::unique_ptr<Bakers::IMapBaker> CreateBaker() const noexcept;
		[[nodiscard]] EMapType GetMapType() const noexcept;

	private:
		void LoadSettings();
		void SaveSettings();
		void SetupUI();
		void SetupConnections();

	private:
		Settings::ISettingsManager* m_pSettingsManager = nullptr;
		Settings::SVegetationBakerSettings m_settings;

		QSpinBox* m_pSensitivitySpinBox = nullptr;
		QCheckBox* m_pGrassCheckBox = nullptr;
		QCheckBox* m_pBushCheckBox = nullptr;
		QCheckBox* m_pTreeCheckBox = nullptr;
		QLineEdit* m_pGrassLineEdit = nullptr;
		QLineEdit* m_pBushLineEdit = nullptr;
		QLineEdit* m_pTreeLineEdit = nullptr;
	};
}