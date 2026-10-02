#include "StdAfx.h"
#include "CoordinateAxes.h"

#include <QRectF>
#include <QPainter>
#include <QPalette>
#include <QPolygonF>

namespace JDKLevelMaps::Components
{
	QMargins CCoordinateAxes::CalculateMargins(const QFontMetrics& metrics) const
	{
		const int originTextWidth = metrics.horizontalAdvance(m_originLabel);

		const int left = m_gap + originTextWidth / 2 + m_textPadding;
		const int top = m_arrowLength;
		const int right = m_arrowLength;
		const int bottom = m_gap + m_textPadding + metrics.height();

		return QMargins(left, top, right, bottom);
	}

	void CCoordinateAxes::Paint(QPainter& painter, const QRectF& contentRect, const QPalette& palette) const
	{
		const QColor color = palette.color(QPalette::Text);

		painter.save();
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen(color, m_lineWidth, Qt::SolidLine, Qt::FlatCap));
		painter.setBrush(color);

		const QPointF origin(contentRect.left() - m_gap, contentRect.bottom() + m_gap);
		const QPointF xTip(contentRect.right() + m_arrowLength, origin.y());
		const QPointF yTip(origin.x(), contentRect.top() - m_arrowLength);

		painter.drawLine(origin, QPointF(xTip.x() - m_arrowLength, xTip.y()));
		painter.drawLine(origin, QPointF(yTip.x(), yTip.y() + m_arrowLength));

		const QPolygonF xArrow({ xTip, QPointF(xTip.x() - m_arrowLength, xTip.y() - m_arrowHalfWidth), QPointF(xTip.x() - m_arrowLength, xTip.y() + m_arrowHalfWidth) });
		const QPolygonF yArrow({ yTip, QPointF(yTip.x() - m_arrowHalfWidth, yTip.y() + m_arrowLength), QPointF(yTip.x() + m_arrowHalfWidth, yTip.y() + m_arrowLength) });

		painter.drawPolygon(xArrow);
		painter.drawPolygon(yArrow);

		const QFontMetrics metrics(painter.font());
		const qreal textTop = origin.y() + m_textPadding;

		const qreal originW = metrics.horizontalAdvance(m_originLabel);
		painter.drawText(QRectF(origin.x() - originW / 2, textTop, originW, metrics.height()), Qt::AlignCenter, m_originLabel);

		const qreal xW = metrics.horizontalAdvance(m_xLabel);
		painter.drawText(QRectF(xTip.x() - xW, textTop, xW, metrics.height()), Qt::AlignCenter, m_xLabel);

		const qreal yW = metrics.horizontalAdvance(m_yLabel);
		painter.drawText(QRectF(yTip.x() - m_textPadding - yW, yTip.y(), yW, metrics.height()), Qt::AlignCenter, m_yLabel);

		painter.restore();
	}
}