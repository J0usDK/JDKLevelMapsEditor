#pragma once
#include <QPushButton>

namespace JDKLevelMaps::Components
{
	enum class EButtonState : uint8
	{
		Start = 0,
		Stop,
		Cancelling,

		Count
	};

	class CGenerateButton : public QPushButton
	{
		Q_OBJECT
	public:
		CGenerateButton(QWidget* pParent = nullptr);

		void SetButtonState(EButtonState state);
		[[nodiscard]] EButtonState GetButtonState() const noexcept;

		void SetText(EButtonState, const QString& str);
		void SetToolTip(EButtonState, const QString& str);

	signals:
		void buttonClicked(EButtonState state);

	private:
		EButtonState m_currentState = EButtonState::Start;

		std::array<QString, static_cast<size_t>(EButtonState::Count)> m_stateTexts;
		std::array<QString, static_cast<size_t>(EButtonState::Count)> m_stateToolTips;
	};
}