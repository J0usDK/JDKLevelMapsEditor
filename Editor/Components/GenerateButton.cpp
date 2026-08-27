#include "StdAfx.h"
#include "GenerateButton.h"

namespace JDKLevelMaps::Components
{
	CGenerateButton::CGenerateButton(QWidget* pParent) : QPushButton(pParent)
	{
		connect(this, &QPushButton::clicked, this, [this](bool /*checked*/)
			{ Q_EMIT buttonClicked(m_currentState); });
	}

	void CGenerateButton::SetButtonState(EButtonState state)
	{
		CRY_ASSERT(state < EButtonState::Count, "[JDKLevelMaps] Button got wrong state");

		m_currentState = state;
		setText(m_stateTexts[static_cast<size_t>(state)]);
		setToolTip(m_stateToolTips[static_cast<size_t>(state)]);
	}

	void CGenerateButton::SetText(EButtonState state, const QString& str)
	{
		CRY_ASSERT(state < EButtonState::Count, "[JDKLevelMaps] Button got wrong state");

		m_stateTexts[static_cast<size_t>(state)] = str;
		if (m_currentState == state)
			setText(str);
	}

	void CGenerateButton::SetToolTip(EButtonState state, const QString& str)
	{
		CRY_ASSERT(state < EButtonState::Count, "[JDKLevelMaps] Button got wrong state");

		m_stateToolTips[static_cast<size_t>(state)] = str;
		if (m_currentState == state)
			setToolTip(str);
	}

	EButtonState CGenerateButton::GetButtonState() const noexcept { return m_currentState; }
}