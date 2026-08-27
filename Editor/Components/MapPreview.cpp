#include "StdAfx.h"
#include "MapPreview.h"

#include <QPainter>
#include <QWheelEvent>

#include "GenerateButton.h"

namespace JDKLevelMaps::Components
{
	CMapPreview::CMapPreview(QWidget* pParent) : QLabel(pParent)
	{
		setAlignment(Qt::AlignCenter);
		setMinimumSize(200, 200);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

		m_pLoadButton = new CGenerateButton(this);
		m_pLoadButton->SetButtonState(EButtonState::Start);
		m_pLoadButton->SetText(EButtonState::Start, tr("Load Preview"));
		m_pLoadButton->SetText(EButtonState::Stop, tr("Stop"));
		m_pLoadButton->SetText(EButtonState::Cancelling, tr("Cancelling..."));
		m_pLoadButton->SetToolTip(EButtonState::Start, tr("Generates a preview from the baked map in memory, without saving it on disk.\n"
			"Warning: This operation may take a long time and consume a lot of RAM with large maps."));

		m_pLoadButton->setFixedSize(180, 25);
		m_pLoadButton->hide();

		connect(m_pLoadButton, &CGenerateButton::buttonClicked, this, [this](EButtonState state)
			{ Q_EMIT loadPreviewClicked(state == EButtonState::Start); });
	}

	void CMapPreview::SetPixmap(const QPixmap& pixmap)
	{
		m_pixmap = pixmap;
		m_bHasPixmap = !m_pixmap.isNull();
		ResetView();
	}

	void CMapPreview::ResetPixmap(const QString& text)
	{
		SetPixmap(QPixmap());
		setText(text);
	}

	bool CMapPreview::HasPixmap() const noexcept { return m_bHasPixmap; }

	void CMapPreview::EnableLoadButton(bool bEnable)
	{
		m_pLoadButton->setEnabled(bEnable);
	}

	void CMapPreview::ShowLoadButton(bool bShow)
	{
		m_pLoadButton->setVisible(bShow);
	}

	void CMapPreview::ShowLoadButton(EButtonState state, bool bShow)
	{
		m_pLoadButton->SetButtonState(state);
		ShowLoadButton(bShow);
	}

	void CMapPreview::paintEvent(QPaintEvent* pEvent)
	{
		if (m_pixmap.isNull())
		{
			QLabel::paintEvent(pEvent);
			return;
		}

		QPainter painter(this);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, m_zoomFactor <= 1.0f);

		QSizeF targetSize = m_pixmap.size();
		targetSize.scale(size(), Qt::KeepAspectRatio);
		targetSize *= m_zoomFactor;

		QRectF targetRect(
			(width() - targetSize.width()) / 2.0f + m_offset.x(),
			(height() - targetSize.height()) / 2.0f + m_offset.y(),
			targetSize.width(),
			targetSize.height());

		painter.drawPixmap(targetRect, m_pixmap, m_pixmap.rect());
	}

	void CMapPreview::resizeEvent(QResizeEvent* pEvent)
	{
		QLabel::resizeEvent(pEvent);

		int x = (width() - m_pLoadButton->width()) / 2;
		int y = (height() - m_pLoadButton->height()) / 2 + 25;
		m_pLoadButton->move(x, y);
	}

	void CMapPreview::mousePressEvent(QMouseEvent* pEvent)
	{
		if (pEvent->button() == Qt::MiddleButton)
		{
			m_bIsDragging = true;
			m_lastMousePos = pEvent->pos();
			setCursor(Qt::ClosedHandCursor);
			pEvent->accept();
		}
		else
			QLabel::mousePressEvent(pEvent);
	}

	void CMapPreview::mouseMoveEvent(QMouseEvent* pEvent)
	{
		if (m_bIsDragging)
		{
			QPoint delta = pEvent->pos() - m_lastMousePos;
			m_offset += delta;
			ClampOffset();
			m_lastMousePos = pEvent->pos();

			update();
			pEvent->accept();
		}
		else
			QLabel::mouseMoveEvent(pEvent);
	}

	void CMapPreview::mouseReleaseEvent(QMouseEvent* pEvent)
	{
		if (pEvent->button() == Qt::MiddleButton && m_bIsDragging)
		{
			m_bIsDragging = false;
			unsetCursor();
			pEvent->accept();
		}
		else
			QLabel::mouseReleaseEvent(pEvent);
	}

	void CMapPreview::mouseDoubleClickEvent(QMouseEvent* pEvent)
	{
		if (pEvent->button() == Qt::LeftButton)
		{
			ResetView();
			pEvent->accept();
		}
		else
			QLabel::mouseDoubleClickEvent(pEvent);
	}

	void CMapPreview::wheelEvent(QWheelEvent* pEvent)
	{
		if (pEvent->modifiers() & Qt::ControlModifier)
		{
			const int delta = pEvent->angleDelta().y();
			if (delta == 0)
			{
				pEvent->accept();
				return;
			}

			const float oldZoom = m_zoomFactor;
			const float step = delta > 0 ? 1.25f : 0.8f;
			const float maxZoom = GetMaxZoom();
			m_zoomFactor = std::clamp(m_zoomFactor * step, 1.0f, maxZoom);

			if (oldZoom != m_zoomFactor)
			{
				if (m_zoomFactor == 1.0f)
					m_offset = {};
				else
				{
					QPointF mousePos = pEvent->posF();
					QPointF widgetCenter(width() / 2.0f, height() / 2.0f);
					QPointF vectorToMouse = mousePos - (widgetCenter + m_offset);

					float ratio = m_zoomFactor / oldZoom;
					m_offset = mousePos - widgetCenter - (vectorToMouse * ratio);

					ClampOffset();
				}
			}
			else
				ClampOffset();

			update();
			pEvent->accept();
		}
		else
			QLabel::wheelEvent(pEvent);
	}

	void CMapPreview::ResetView() noexcept
	{
		m_zoomFactor = 1.0f;
		m_offset = {};
		update();
	}

	void CMapPreview::ClampOffset() noexcept
	{
		if (m_pixmap.isNull())
			return;

		QSizeF targetSize = m_pixmap.size();
		targetSize.scale(size(), Qt::KeepAspectRatio);
		targetSize *= m_zoomFactor;

		const double halfWidgetWidth = width() * 0.5;
		const double halfWidgetHeight = height() * 0.5;

		const double halfImageWidth = targetSize.width() * 0.5;
		const double halfImageHeight = targetSize.height() * 0.5;

		const double maxOffsetX = std::max(0.0, halfImageWidth - halfWidgetWidth);
		const double maxOffsetY = std::max(0.0, halfImageHeight - halfWidgetHeight);

		m_offset.setX(std::clamp(m_offset.x(), -maxOffsetX, maxOffsetX));
		m_offset.setY(std::clamp(m_offset.y(), -maxOffsetY, maxOffsetY));
	}

	float CMapPreview::GetMaxZoom() const noexcept
	{
		if (m_pixmap.isNull())
			return 1.0f;

		const int maxDimension = std::max(m_pixmap.width(), m_pixmap.height());

		return std::clamp(static_cast<float>(maxDimension) / 1024.0f * 4.0f, 16.0f, 128.0f);
	}
}