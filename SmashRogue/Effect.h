#pragma once
#include "Common.h"
#include <string>
#include <vector>

// パーティクル・ダメージ数字・画面揺れなどの演出
class EffectManager {
public:
	void Clear();
	void Update();
	void Draw(const Vec2& off, int font, int bigFont) const;

	void Burst(const Vec2& pos, unsigned int color, int count, float speed, float size, int life);
	void Text(const Vec2& pos, const std::string& text, unsigned int color, bool big = false);
	void Ring(const Vec2& pos, float maxRadius, unsigned int color, int life);
	void Lightning(const Vec2& pos);
	void Shake(float power);

	Vec2 ShakeOffset() const { return shakeOffset_; }

private:
	struct Particle {
		Vec2 pos;
		Vec2 vel;
		unsigned int color;
		float size;
		int life;
		int maxLife;
	};
	struct FloatText {
		Vec2 pos;
		std::string text;
		unsigned int color;
		int life;
		bool big;
	};
	struct RingFx {
		Vec2 pos;
		float maxRadius;
		unsigned int color;
		int life;
		int maxLife;
	};
	struct Bolt {
		std::vector<Vec2> points;
		int life;
	};

	std::vector<Particle> particles_;
	std::vector<FloatText> texts_;
	std::vector<RingFx> rings_;
	std::vector<Bolt> bolts_;
	float shake_ = 0.0f;
	Vec2 shakeOffset_;
};
