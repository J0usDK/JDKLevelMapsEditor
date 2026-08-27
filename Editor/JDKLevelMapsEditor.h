#pragma once
#include <EditorFramework/Editor.h>

class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QProgressBar;

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

class CJDKLevelMapsEditor final : public CDockableEditor, public IAutoEditorNotifyListener
{
	Q_OBJECT
public:
	CJDKLevelMapsEditor(QWidget* pParent = nullptr);
	~CJDKLevelMapsEditor() = default;

	void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	const char* GetEditorName() const noexcept override;

private slots:
	void OnGenerateButtonClicked(JDKLevelMaps::Components::EButtonState clickedState);
	void OnLoadPreviewButtonClicked(bool bStart);
	void OnCellSizeChanged(double value);
	void OnOperationStateChanged();
	void OnBakeFinished(bool bSuccess, QString message);
	void OnPreviewAvailabilityChanged(bool bHasMap, bool bHasImage, QString imagePath);
	void OnPreviewLoaded(QImage image);
	void OnPreviewLoadFailed(QString message);

private:
	void UpdateUIState();
	void UpdateLevelState(bool bLevelLoaded);
	void SetupWidget(QWidget* pWidget);
	void SetupConnections();

	void LoadSettings();
	void LoadVegetationSettings(JDKLevelMaps::Settings::SVegetationBakerSettings& vegSettings);

	void SaveSettings();
	void SaveVegetationSettings(const JDKLevelMaps::Settings::SVegetationBakerSettings& vegSettings);

	void ShowError(const QString& context, const QString& errorMsg);

private:
	QWidget* m_pRootWidget = nullptr;
	QDoubleSpinBox* m_pCellSizeSpinBox = nullptr;
	QSpinBox* m_pTileSizeSpinBox = nullptr;
	QSpinBox* m_pSensitivitySpinBox = nullptr;
	QCheckBox* m_pGrassCheckBox = nullptr;
	QCheckBox* m_pBushCheckBox = nullptr;
	QCheckBox* m_pTreeCheckBox = nullptr;
	QCheckBox* m_pGenerateImageCheckBox = nullptr;
	QLineEdit* m_pGrassLineEdit = nullptr;
	QLineEdit* m_pBushLineEdit = nullptr;
	QLineEdit* m_pTreeLineEdit = nullptr;
	QProgressBar* m_pProgressBar = nullptr;
	JDKLevelMaps::Components::CMapPreview* m_pMapPreview = nullptr;
	JDKLevelMaps::Components::CGenerateButton* m_pGenerateButton = nullptr;

	JDKLevelMaps::ViewModels::CLevelMapsViewModel* m_pViewModel = nullptr;

	bool m_bLevelLoaded = false;
	bool m_bHasMap = false;
};