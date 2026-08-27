#pragma once
#include <QLabel>

namespace JDKLevelMaps::Components
{
	class CGenerateButton;
	enum class EButtonState : uint8;

	class CMapPreview : public QLabel
	{
		Q_OBJECT
	public:
		CMapPreview(QWidget* pParent = nullptr);

		void SetPixmap(const QPixmap& pixmap);
		void ResetPixmap(const QString& text);
		bool HasPixmap() const noexcept;

		void EnableLoadButton(bool bEnable);
		void ShowLoadButton(bool bShow);
		void ShowLoadButton(EButtonState buttonState, bool bShow);

	protected:
		void paintEvent(QPaintEvent* pEvent) override;
		void resizeEvent(QResizeEvent* pEvent) override;
		void mousePressEvent(QMouseEvent* pEvent) override;
		void mouseMoveEvent(QMouseEvent* pEvent) override;
		void mouseReleaseEvent(QMouseEvent* pEvent) override;
		void mouseDoubleClickEvent(QMouseEvent* pEvent) override;
		void wheelEvent(QWheelEvent* pEvent) override;

	signals:
		void loadPreviewClicked(bool bStart);

	private:
		void ResetView() noexcept;
		void ClampOffset() noexcept;
		[[nodiscard]] float GetMaxZoom() const noexcept;

	private:
		QPixmap m_pixmap;
		bool m_bHasPixmap = false;
		float m_zoomFactor = 1.0f;

		CGenerateButton* m_pLoadButton = nullptr;

		QPointF m_offset;
		QPoint m_lastMousePos;
		bool m_bIsDragging = false;
	};
}