#include "Game.h"
#include "DxLib.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
Fonts CreateFonts()
{
	Fonts f{};
	f.mini = CreateFontToHandle(nullptr, 18, 3, DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 2);
	f.normal = CreateFontToHandle(nullptr, 24, 4, DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 2);
	f.big = CreateFontToHandle(nullptr, 36, 6, DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 3);
	f.huge = CreateFontToHandle(nullptr, 72, 8, DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 4);
	return f;
}
}

Game::Game()
	: fonts_(CreateFonts()), play_(fonts_)
{
}

Game::~Game()
{
	DeleteFontToHandle(fonts_.mini);
	DeleteFontToHandle(fonts_.normal);
	DeleteFontToHandle(fonts_.big);
	DeleteFontToHandle(fonts_.huge);
}

void Game::Update()
{
	input_.Update();
	++timer_;

	switch (scene_) {
	case Scene::Title:
		if (input_.IsTrigger(Button::Confirm)) {
			play_.Start();
			scene_ = Scene::Play;
		}
		if (input_.IsTrigger(Button::Pause)) quit_ = true;
		break;
	case Scene::Play:
		play_.Update(input_);
		if (play_.IsQuitRequested()) {
			scene_ = Scene::Title;
		} else if (play_.IsGameOver()) {
			result_ = play_.GetResult();
			bestFloor_ = std::max(bestFloor_, result_.floor);
			scene_ = Scene::Result;
			timer_ = 0;
		}
		break;
	case Scene::Result:
		if (timer_ > 40 && input_.IsTrigger(Button::Confirm)) scene_ = Scene::Title;
		break;
	}
}

void Game::Draw() const
{
	switch (scene_) {
	case Scene::Title:
		DrawTitle();
		break;
	case Scene::Play:
		play_.Draw();
		break;
	case Scene::Result:
		DrawResult();
		break;
	}
}

void Game::DrawCentered(const char* text, int y, unsigned int color, int font) const
{
	int width = GetDrawStringWidthToHandle(text, static_cast<int>(std::strlen(text)), font);
	DrawStringToHandle((kScreenW - width) / 2, y, text, color, font, GetColor(0, 0, 0));
}

void Game::DrawTitle() const
{
	// 背景のグラデーション
	for (int y = 0; y < kScreenH; y += 8) {
		int c = 10 + y * 30 / kScreenH;
		DrawBox(0, y, kScreenW, y + 8, GetColor(c, c / 2 + 6, c + 18), TRUE);
	}

	// 跳ねるスライムの飾り
	for (int i = 0; i < 7; ++i) {
		float x = 140.0f + i * 167.0f;
		float hop = std::max(0.0f, std::sin(timer_ * 0.08f + i * 1.3f));
		float y = 600.0f - hop * 40.0f;
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90);
		DrawOvalAA(x, 618.0f, 18.0f, 6.0f, 16, GetColor(0, 0, 0), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawOvalAA(x, y, 20.0f + (1.0f - hop) * 3.0f, 17.0f, 32, GetColor(40, 110, 220), TRUE);
		DrawTriangleAA(x - 6, y - 14, x + 6, y - 14, x + 1, y - 27, GetColor(40, 110, 220), TRUE);
		DrawCircleAA(x - 5, y - 1, 2.6f, 10, GetColor(20, 20, 30), TRUE);
		DrawCircleAA(x + 5, y - 1, 2.6f, 10, GetColor(20, 20, 30), TRUE);
		DrawOvalAA(x, y + 6, 5.0f, 2.5f, 12, GetColor(200, 40, 60), TRUE);
	}

	DrawCentered("スマッシュローグ", 130, GetColor(255, 220, 100), fonts_.huge);
	DrawCentered("～ 斬って 育てて 深く もぐれ ～", 230, GetColor(220, 220, 240), fonts_.normal);

	if ((timer_ / 30) % 2 == 0) {
		DrawCentered("Z / Enter で スタート", 320, GetColor(255, 255, 255), fonts_.big);
	}

	const char* lines[] = {
		"いどう : WASD / 方向キー / 左スティック",
		"こうげき : Z / J / 左クリック（おしっぱなしで連続）",
		"ダッシュ : X / Space / 右クリック（無敵）",
		"ポーズ : Esc / P        タイトルで Esc : 終了",
	};
	for (int i = 0; i < 4; ++i) {
		DrawCentered(lines[i], 400 + i * 30, GetColor(190, 200, 220), fonts_.mini);
	}

	if (bestFloor_ > 0) {
		DrawFormatStringToHandle(20, kScreenH - 36, GetColor(255, 230, 140), fonts_.mini, "さいこう到達 : B%dF", bestFloor_);
	}
}

void Game::DrawResult() const
{
	DrawBox(0, 0, kScreenW, kScreenH, GetColor(20, 6, 10), TRUE);
	DrawCentered("ちからつきた…", 120, GetColor(255, 110, 100), fonts_.huge);

	char buf[128];
	snprintf(buf, sizeof(buf), "到達フロア   B%dF", result_.floor);
	DrawCentered(buf, 260, GetColor(255, 230, 140), fonts_.big);
	snprintf(buf, sizeof(buf), "レベル   %d", result_.level);
	DrawCentered(buf, 320, GetColor(230, 230, 240), fonts_.big);
	snprintf(buf, sizeof(buf), "たおした数   %d", result_.kills);
	DrawCentered(buf, 380, GetColor(230, 230, 240), fonts_.big);
	snprintf(buf, sizeof(buf), "プレイ時間   %d:%02d", result_.frames / 3600, result_.frames / 60 % 60);
	DrawCentered(buf, 440, GetColor(230, 230, 240), fonts_.big);

	if (timer_ > 40 && (timer_ / 30) % 2 == 0) {
		DrawCentered("Z / Enter で タイトルへ", 560, GetColor(255, 255, 255), fonts_.normal);
	}
}
