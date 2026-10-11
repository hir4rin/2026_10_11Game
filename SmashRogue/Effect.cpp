#include "Effect.h"
#include "Render3D.h"
#include "DxLib.h"
#include <algorithm>

void EffectManager::Clear()
{
	particles_.clear();
	texts_.clear();
	rings_.clear();
	bolts_.clear();
	shake_ = 0.0f;
	shakeOffset_ = {};
}

void EffectManager::Update()
{
	for (auto& p : particles_) {
		p.pos += p.vel;
		p.vel *= 0.9f;
		// 跳ね上がって落ちる
		p.height += p.vh;
		p.vh -= 0.35f;
		if (p.height < 2.0f) {
			p.height = 2.0f;
			p.vh *= -0.4f;
		}
		--p.life;
	}
	for (auto& t : texts_) {
		t.rise += t.life > 30 ? 1.2f : 0.3f;
		--t.life;
	}
	for (auto& r : rings_) --r.life;
	for (auto& b : bolts_) --b.life;

	auto dead = [](const auto& e) { return e.life <= 0; };
	particles_.erase(std::remove_if(particles_.begin(), particles_.end(), dead), particles_.end());
	texts_.erase(std::remove_if(texts_.begin(), texts_.end(), dead), texts_.end());
	rings_.erase(std::remove_if(rings_.begin(), rings_.end(), dead), rings_.end());
	bolts_.erase(std::remove_if(bolts_.begin(), bolts_.end(), dead), bolts_.end());

	shake_ *= 0.85f;
	if (shake_ < 0.3f) shake_ = 0.0f;
	shakeOffset_ = Vec2(RandF(-shake_, shake_), RandF(-shake_, shake_));
}

void EffectManager::Draw3D() const
{
	for (const auto& r : rings_) {
		float t = 1.0f - static_cast<float>(r.life) / r.maxLife;
		float radius = r.maxRadius * (0.2f + 0.8f * t);
		r3d::GroundRing(r.pos, radius, std::max(4.0f, radius * 0.12f), r.color, static_cast<int>(255 * (1.0f - t)), true, 2.0f);
	}
}

void EffectManager::Draw2D(int font, int bigFont) const
{
	for (const auto& b : bolts_) {
		SetDrawBlendMode(DX_BLENDMODE_ADD, std::min(255, b.life * 30));
		for (size_t i = 1; i < b.points.size(); ++i) {
			const Vec2 a = r3d::Project(b.points[i - 1].pos.x, b.points[i - 1].height, b.points[i - 1].pos.y);
			const Vec2 c = r3d::Project(b.points[i].pos.x, b.points[i].height, b.points[i].pos.y);
			DrawLineAA(a.x, a.y, c.x, c.y, GetColor(255, 240, 120), 8.0f);
			DrawLineAA(a.x, a.y, c.x, c.y, GetColor(255, 255, 255), 3.0f);
		}
	}
	for (const auto& p : particles_) {
		float t = static_cast<float>(p.life) / p.maxLife;
		const Vec2 s = r3d::Project(p.pos.x, p.height, p.pos.y);
		SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(255 * t));
		DrawCircleAA(s.x, s.y, std::max(0.5f, p.size * (0.4f + 0.6f * t)), 8, p.color, TRUE);
	}
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	for (const auto& t : texts_) {
		int handle = t.big ? bigFont : font;
		int width = GetDrawStringWidthToHandle(t.text.c_str(), static_cast<int>(t.text.size()), handle);
		const Vec2 s = r3d::Project(t.pos.x, 40.0f, t.pos.y);
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::min(255, t.life * 12));
		DrawStringToHandle(static_cast<int>(s.x) - width / 2, static_cast<int>(s.y - t.rise),
			t.text.c_str(), t.color, handle, GetColor(20, 20, 30));
	}
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void EffectManager::Burst(const Vec2& pos, unsigned int color, int count, float speed, float size, int life)
{
	for (int i = 0; i < count; ++i) {
		Vec2 vel = Vec2::FromAngle(RandF(0.0f, kPi * 2.0f)) * RandF(speed * 0.3f, speed);
		int l = RandI(life / 2, life);
		particles_.push_back({ pos, vel, 16.0f, RandF(1.0f, speed * 0.8f), color, RandF(size * 0.5f, size), l, l });
	}
}

void EffectManager::Text(const Vec2& pos, const std::string& text, unsigned int color, bool big)
{
	texts_.push_back({ pos + Vec2(RandF(-8.0f, 8.0f), 0.0f), 0.0f, text, color, big ? 60 : 42, big });
}

void EffectManager::Ring(const Vec2& pos, float maxRadius, Rgb color, int life)
{
	rings_.push_back({ pos, maxRadius, color, life, life });
}

void EffectManager::Lightning(const Vec2& pos)
{
	// 空から地面へジグザグに落ちる
	Bolt bolt;
	bolt.life = 14;
	const int segments = 8;
	for (int i = 0; i <= segments; ++i) {
		float t = static_cast<float>(i) / segments;
		float j = (i != 0 && i != segments) ? 22.0f : 0.0f;
		bolt.points.push_back({ pos + Vec2(RandF(-j - 0.01f, j + 0.01f), RandF(-j - 0.01f, j + 0.01f)), 420.0f * (1.0f - t) });
	}
	bolts_.push_back(bolt);
	Ring(pos, 70.0f, Rgb{ 255, 240, 120 }, 18);
	Burst(pos, GetColor(255, 250, 180), 14, 7.0f, 5.0f, 24);
}

void EffectManager::Shake(float power)
{
	shake_ = std::max(shake_, power);
}
