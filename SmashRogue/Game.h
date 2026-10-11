#pragma once
#include "Input.h"
#include "PlayScene.h"

// タイトル → プレイ → リザルト の流れを管理
class Game {
public:
	Game();
	~Game();

	void Update();
	void Draw() const;
	bool IsQuit() const { return quit_; }

private:
	enum class Scene { Title, Play, Result };

	void DrawTitle() const;
	void DrawResult() const;
	void DrawCentered(const char* text, int y, unsigned int color, int font) const;

	Fonts fonts_;
	Input input_;
	PlayScene play_;
	Scene scene_ = Scene::Title;
	RunResult result_{};
	int bestFloor_ = 0;
	int timer_ = 0;
	bool quit_ = false;
};
