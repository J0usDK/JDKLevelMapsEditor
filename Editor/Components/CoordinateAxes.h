#pragma once
#include <QMargins>
#include <QString>

class QRectF;
class QPainter;
class QPalette;
class QFontMetrics;

namespace JDKLevelMaps::Components
{
	class CCoordinateAxes final
	{
	public:
		[[nodiscard]] QMargins CalculateMargins(const QFontMetrics& metrics) const;

		void Paint(QPainter& painter, const QRectF& contentRect, const QPalette& palette) const;

	private:
		qreal m_lineWidth = 1.0;
		int m_gap = 4;
		int m_arrowLength = 9;
		int m_arrowHalfWidth = 3;
		int m_textPadding = 2;

		QString m_originLabel = QStringLiteral("(0, 0)");
		QString m_xLabel = QStringLiteral("X");
		QString m_yLabel = QStringLiteral("Y");
	};
}