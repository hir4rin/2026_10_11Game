#include "Input.h"
#include "DxLib.h"

void Input::Update()
{
	char keys[256] = {};
	GetHitKeyStateAll(keys);
	const int pad = GetJoypadInputState(DX_INPUT_PAD1);
	const int mouse = GetMouseInput();

	auto key = [&](int code) { return keys[code] != 0; };
	auto padOn = [&](int mask) { return (pad & mask) != 0; };

	for (int i = 0; i < static_cast<int>(Button::Count); ++i) {
		prev_[i] = now_[i];
	}

	auto set = [&](Button b, bool on) { now_[static_cast<int>(b)] = on; };
	set(Button::Up, key(KEY_INPUT_W) || key(KEY_INPUT_UP) || padOn(PAD_INPUT_UP));
	set(Button::Down, key(KEY_INPUT_S) || key(KEY_INPUT_DOWN) || padOn(PAD_INPUT_DOWN));
	set(Button::Left, key(KEY_INPUT_A) || key(KEY_INPUT_LEFT) || padOn(PAD_INPUT_LEFT));
	set(Button::Right, key(KEY_INPUT_D) || key(KEY_INPUT_RIGHT) || padOn(PAD_INPUT_RIGHT));
	set(Button::Attack, key(KEY_INPUT_Z) || key(KEY_INPUT_J) || padOn(PAD_INPUT_1) || (mouse & MOUSE_INPUT_LEFT) != 0);
	set(Button::Dash, key(KEY_INPUT_X) || key(KEY_INPUT_K) || key(KEY_INPUT_SPACE) || key(KEY_INPUT_LSHIFT)
		|| padOn(PAD_INPUT_2) || (mouse & MOUSE_INPUT_RIGHT) != 0);
	set(Button::Confirm, key(KEY_INPUT_Z) || key(KEY_INPUT_RETURN) || key(KEY_INPUT_SPACE) || padOn(PAD_INPUT_1));
	set(Button::Pause, key(KEY_INPUT_ESCAPE) || key(KEY_INPUT_P) || padOn(PAD_INPUT_8));
	set(Button::Quit, key(KEY_INPUT_Q));

	// デジタル入力
	Vec2 axis;
	if (IsPress(Button::Left)) axis.x -= 1.0f;
	if (IsPress(Button::Right)) axis.x += 1.0f;
	if (IsPress(Button::Up)) axis.y -= 1.0f;
	if (IsPress(Button::Down)) axis.y += 1.0f;

	// アナログスティック（デジタル入力が無い時だけ）
	if (axis.LengthSq() == 0.0f) {
		int ax = 0;
		int ay = 0;
		GetJoypadAnalogInput(&ax, &ay, DX_INPUT_PAD1);
		Vec2 stick(ax / 1000.0f, ay / 1000.0f);
		if (stick.Length() > 0.2f) axis = stick;
	}
	axis_ = axis.Length() > 1.0f ? axis.Normalized() : axis;
}
