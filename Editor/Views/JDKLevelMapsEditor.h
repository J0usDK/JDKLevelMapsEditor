#pragma once
#include <EditorFramework/Editor.h>

#include "Settings/ISettingsManager.h"

class QTabBar;
class QStackedWidget;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QProgressBar;
class QVariant;

namespace JDKLevelMaps
{
	enum class EMapType : uint8;
}

namespace JDKLevelMaps::Components
{
	class CMapPreview;
	class CGenerateButton;
	enum class EButtonState : uint8;
}

namespace JDKLevelMaps::Settings
{
	struct SVegetationBakerSettings;
}

namespace JDKLevelMaps::ViewModels
{
	class CLevelMapsViewModel;
}

namespace JDKLevelMaps::Views
{
	class CJDKLevelMapsEditor final : public CDockableEditor, public IAutoEditorNotifyListener, public Settings::ISettingsManager
	{
		Q_OBJECT
	public:
		CJDKLevelMapsEditor(QWidget* pParent = nullptr);
		~CJDKLevelMapsEditor() = default;

		void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
		const char* GetEditorName() const noexcept override;

		void SetPluginProperty(const char* key, const QVariant& value) override;
		QVariant GetPluginProperty(const char* key) override;

	private slots:
		void OnGenerateButtonClicked(Components::EButtonState clickedState);
		void OnLoadPreviewButtonClicked(bool bStart);
		void OnCellSizeChanged(double value);
		void OnOperationStateChanged();
		void OnBakeFinished(bool bSuccess, QString message);
		void OnPreviewAvailabilityChanged(bool bHasMap, bool bHasImage);
		void OnPreviewLoaded(QImage image);
		void OnPreviewLoadFailed(QString message);

	private:
		void UpdateUIState();
		void UpdateLevelState(bool bLevelLoaded);
		void SetupWidget(QWidget* pWidget);
		void SetupConnections();

		void LoadSettings();
		void SaveSettings();

		void ShowError(const QString& context, const QString& errorMsg);

		[[nodiscard]] EMapType GetActiveMapType() const noexcept;

	private:
		QWidget* m_pRootWidget = nullptr;

		QTabBar* m_pTabBar = nullptr;
		QStackedWidget* m_pStackedWidget = nullptr;

		QComboBox* m_pFormatComboBox = nullptr;
		QDoubleSpinBox* m_pCellSizeSpinBox = nullptr;
		QSpinBox* m_pTileSizeSpinBox = nullptr;
		QCheckBox* m_pGenerateImageCheckBox = nullptr;
		QProgressBar* m_pProgressBar = nullptr;

		Components::CMapPreview* m_pMapPreview = nullptr;
		Components::CGenerateButton* m_pGenerateButton = nullptr;

		ViewModels::CLevelMapsViewModel* m_pViewModel = nullptr;

		bool m_bLevelLoaded = false;
		bool m_bHasMap = false;
	};
}