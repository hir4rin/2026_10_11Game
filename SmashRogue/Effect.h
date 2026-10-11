#pragma once
#include "Common.h"
#include <string>
#include <vector>

// パーティクル・ダメージ数字・画面揺れなどの演出。
// 位置は地面上の座標 (x, y) と高さで持ち、描画時に画面へ投影する
class EffectManager {
public:
	void Clear();
	void Update();
	void Draw3D() const;                    // 地面の輪（3D 描画中に呼ぶ）
	void Draw2D(int font, int bigFont) const; // 粒子・雷・文字（3D 描画の後に呼ぶ）

	void Burst(const Vec2& pos, unsigned int color, int count, float speed, float size, int life);
	void Text(const Vec2& pos, const std::string& text, unsigned int color, bool big = false);
	void Ring(const Vec2& pos, float maxRadius, Rgb color, int life);
	void Lightning(const Vec2& pos);
	void Shake(float power);

	Vec2 ShakeOffset() const { return shakeOffset_; }

private:
	struct Particle {
		Vec2 pos;
		Vec2 vel;
		float height;
		float vh;
		unsigned int color;
		float size;
		int life;
		int maxLife;
	};
	struct FloatText {
		Vec2 pos;
		float rise;
		std::string text;
		unsigned int color;
		int life;
		bool big;
	};
	struct RingFx {
		Vec2 pos;
		float maxRadius;
		Rgb color;
		int life;
		int maxLife;
	};
	struct BoltPoint {
		Vec2 pos;
		float height;
	};
	struct Bolt {
		std::vector<BoltPoint> points;
		int life;
	};

	std::vector<Particle> particles_;
	std::vector<FloatText> texts_;
	std::vector<RingFx> rings_;
	std::vector<Bolt> bolts_;
	float shake_ = 0.0f;
	Vec2 shakeOffset_;
};
