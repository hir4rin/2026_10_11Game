#pragma once
#include "Common.h"

enum class Button {
	Up,
	Down,
	Left,
	Right,
	Attack,
	Dash,
	Confirm,
	Pause,
	Quit,
	Count
};

// キーボード / パッド / マウスをまとめた入力
class Input {
public:
	void Update();

	bool IsPress(Button b) const { return now_[static_cast<int>(b)]; }
	bool IsTrigger(Button b) const { return now_[static_cast<int>(b)] && !prev_[static_cast<int>(b)]; }

	// 移動方向（長さ 0～1）
	Vec2 MoveAxis() const { return axis_; }

private:
	bool now_[static_cast<int>(Button::Count)] = {};
	bool prev_[static_cast<int>(Button::Count)] = {};
	Vec2 axis_;
};
