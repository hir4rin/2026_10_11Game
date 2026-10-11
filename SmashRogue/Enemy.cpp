#include "Enemy.h"
#include "Effect.h"
#include "Render3D.h"
#include "DxLib.h"
#include <algorithm>

namespace {
struct BaseStats {
	float radius;
	int hp;
	int attack;
	float speed;
	int exp;
};

BaseStats GetBaseStats(EnemyType type)
{
	switch (type) {
	case EnemyType::Slime:  return { 16.0f, 22, 8, 1.3f, 2 };
	case EnemyType::Bat:    return { 13.0f, 14, 6, 2.4f, 2 };
	case EnemyType::Mage:   return { 16.0f, 20, 10, 1.5f, 4 };
	case EnemyType::Knight: return { 20.0f, 55, 14, 1.0f, 6 };
	case EnemyType::Golem:  return { 50.0f, 500, 18, 0.9f, 60 };
	}
	return { 16.0f, 10, 5, 1.0f, 1 };
}

Rgb Mix(const Rgb& a, const Rgb& b, float t)
{
	return { static_cast<int>(a.r + (b.r - a.r) * t), static_cast<int>(a.g + (b.g - a.g) * t), static_cast<int>(a.b + (b.b - a.b) * t) };
}
}

Enemy::Enemy(EnemyType t, const Vec2& p, int floor)
	: type(t), pos(p)
{
	const BaseStats base = GetBaseStats(t);
	const float hpScale = 1.0f + 0.32f * (floor - 1);
	const float atkScale = 1.0f + 0.12f * (floor - 1);
	radius = base.radius;
	maxHp = hp = static_cast<int>(base.hp * hpScale);
	attack = static_cast<int>(base.attack * atkScale);
	speed = base.speed * (1.0f + 0.02f * std::min(floor, 15));
	exp = base.exp + floor / 3;
	isBoss = (t == EnemyType::Golem);
	spawnTimer = isBoss ? 90 : 45;
	seed_ = RandF(0.0f, kPi * 2.0f);
	strafe_ = Chance(0.5f) ? 1.0f : -1.0f;
	aiTimer_ = RandI(0, 60);
}

const char* Enemy::Name() const
{
	switch (type) {
	case EnemyType::Slime:  return "スライム";
	case EnemyType::Bat:    return "コウモリ";
	case EnemyType::Mage:   return "まどうし";
	case EnemyType::Knight: return "よろいのきし";
	case EnemyType::Golem:  return "ストーンゴーレム";
	}
	return "";
}

void Enemy::Update(EnemyWorld& world)
{
	anim_ += 1.0f;
	if (hitFlash > 0) --hitFlash;
	if (orbHitCooldown > 0) --orbHitCooldown;
	if (spawnTimer > 0) {
		--spawnTimer;
		return;
	}

	pos += knock;
	knock *= 0.8f;

	const Vec2 toPlayer = world.playerPos - pos;
	const float dist = toPlayer.Length();
	const Vec2 dir = toPlayer.Normalized();

	switch (type) {
	case EnemyType::Slime: {
		float hop = std::max(0.0f, std::sin(anim_ * 0.12f + seed_));
		pos += dir * speed * (0.25f + hop * 1.5f);
		break;
	}
	case EnemyType::Bat: {
		Vec2 side(-dir.y, dir.x);
		pos += dir * speed + side * (std::sin(anim_ * 0.13f + seed_) * 1.6f);
		break;
	}
	case EnemyType::Mage: {
		Vec2 side(-dir.y, dir.x);
		if (dist > 330.0f) pos += dir * speed;
		else if (dist < 210.0f) pos -= dir * speed;
		else pos += side * speed * 0.6f * strafe_;

		if (++aiTimer_ >= 120) {
			aiTimer_ = 0;
			world.bullets.push_back({ pos + dir * radius, dir * 4.2f, 8.0f, attack, 240, false, Rgb{ 200, 90, 255 } });
		}
		break;
	}
	case EnemyType::Knight:
		switch (aiState_) {
		case 0: // 歩き
			pos += dir * speed;
			if (stateTimer_ > 0) --stateTimer_;
			if (dist < 220.0f && stateTimer_ == 0) {
				aiState_ = 1;
				stateTimer_ = 36;
			}
			break;
		case 1: // 溜め（最後の少しの間は向きを固定）
			if (stateTimer_ > 10) aiDir_ = dir;
			if (--stateTimer_ <= 0) {
				aiState_ = 2;
				stateTimer_ = 20;
			}
			break;
		case 2: // 突進
			pos += aiDir_ * 9.5f;
			if (--stateTimer_ <= 0) {
				aiState_ = 0;
				stateTimer_ = 60;
			}
			break;
		}
		break;
	case EnemyType::Golem:
		UpdateGolem(world, dir);
		break;
	}
}

void Enemy::UpdateGolem(EnemyWorld& world, const Vec2& dir)
{
	const bool enraged = hp < maxHp / 2;

	// HP が減ったら手下を呼ぶ
	if (hp < maxHp * 6 / 10 && ++summonTimer_ >= 420) {
		summonTimer_ = 0;
		world.summonRequest += 3;
	}

	switch (aiState_) {
	case 0: // 歩き
		pos += dir * speed * (enraged ? 1.4f : 1.0f);
		if (++aiTimer_ >= (enraged ? 100 : 150)) {
			aiTimer_ = 0;
			aiState_ = Chance(0.5f) ? 1 : 2;
			stateTimer_ = 45;
		}
		break;
	case 1: // 地面たたき → 衝撃波
		if (--stateTimer_ <= 0) {
			world.shockwaves.push_back({ pos, radius, 460.0f, enraged ? 7.0f : 5.5f, attack, false });
			world.effects.Shake(12.0f);
			world.effects.Burst(pos, GetColor(170, 140, 100), 30, 10.0f, 7.0f, 30);
			aiState_ = 3;
			stateTimer_ = 40;
		}
		break;
	case 2: // 全方位弾
		if (--stateTimer_ <= 0) {
			const int count = enraged ? 20 : 14;
			const float baseAngle = seed_ + burstCount_ * 0.2f;
			for (int i = 0; i < count; ++i) {
				Vec2 d = Vec2::FromAngle(baseAngle + kPi * 2.0f * i / count);
				world.bullets.push_back({ pos + d * radius, d * 3.4f, 10.0f, attack * 2 / 3, 300, false, Rgb{ 255, 150, 60 } });
			}
			if (++burstCount_ < (enraged ? 3 : 2)) {
				stateTimer_ = 22;
			} else {
				burstCount_ = 0;
				aiState_ = 3;
				stateTimer_ = 40;
			}
		}
		break;
	case 3: // 硬直
		if (--stateTimer_ <= 0) aiState_ = 0;
		break;
	}
}

float Enemy::TopHeight() const
{
	switch (type) {
	case EnemyType::Slime:  return radius * 2.0f + 14.0f;
	case EnemyType::Bat:    return 58.0f;
	case EnemyType::Mage:   return 74.0f;
	case EnemyType::Knight: return 66.0f;
	case EnemyType::Golem:  return radius * 2.0f + 18.0f;
	}
	return radius * 2.0f;
}

void Enemy::Draw3D(const Vec2& playerPos) const
{
	using r3d::Shape;
	Vec2 forward = (playerPos - pos).Normalized();
	if (forward.LengthSq() == 0.0f) forward = Vec2(0.0f, 1.0f);

	// 出現前は魔法陣から大きくなって出てくる
	float s = 1.0f;
	if (spawnTimer > 0) {
		float t = 1.0f - static_cast<float>(spawnTimer) / (isBoss ? 90 : 45);
		r3d::GroundRing(pos, radius * (1.6f - t * 0.6f), radius * 0.25f, { 200, 80, 255 }, static_cast<int>(220 * t), true, 1.5f);
		if (t < 0.6f) return;
		s = (t - 0.6f) / 0.4f;
	}
	r3d::Shadow(pos, radius * 0.95f * s);

	// 色合い：被弾で白、溜め中は赤や橙に光る
	const bool white = hitFlash > 4;
	float glow = 0.0f;
	Rgb glowColor{};
	if (!white && type == EnemyType::Knight && aiState_ == 1 && (static_cast<int>(anim_) / 3) % 2 == 0) {
		glow = 0.55f;
		glowColor = { 255, 50, 30 };
	}
	if (!white && type == EnemyType::Golem && (aiState_ == 1 || aiState_ == 2)) {
		glow = 0.3f + 0.2f * std::sin(anim_ * 0.5f);
		glowColor = { 255, 120, 40 };
	}
	auto P = [&](Shape shape, float x, float y, float z, float sx, float sy, float sz, Rgb c, float e = 0.0f) {
		Rgb col = white ? Rgb{ 255, 255, 255 } : (glow > 0.0f ? Mix(c, glowColor, glow) : c);
		float em = white ? 1.0f : std::max(e, glow * 0.6f);
		r3d::Part({ pos, forward }, shape, VGet(x * s, y * s, z * s), VGet(sx * s, sy * s, sz * s), col, em);
	};
	const Rgb black{ 20, 20, 32 };

	switch (type) {
	case EnemyType::Slime: {
		float hop = std::max(0.0f, std::sin(anim_ * 0.12f + seed_));
		float squash = 1.0f - hop, lift = hop * 12.0f;
		float rx = radius * (1.0f + 0.18f * squash), ry = radius * (0.95f - 0.12f * squash);
		const Rgb body{ 40, 112, 220 };
		P(Shape::Sphere, 0.0f, ry + lift, 0.0f, rx, ry, rx, body);
		P(Shape::Cone, 0.0f, ry * 2.0f + lift + 3.0f, 0.0f, 5.0f, 11.0f, 5.0f, body);
		P(Shape::Sphere, -rx * 0.35f, ry * 1.55f + lift, rx * 0.1f, 4.0f, 2.5f, 3.0f, { 230, 240, 255 }, 0.8f);
		for (float x : { -5.0f, 5.0f }) P(Shape::Sphere, x, ry + lift + 2.0f, rx * 0.93f, 2.6f, 3.0f, 1.5f, black);
		P(Shape::Sphere, 0.0f, ry + lift - 5.0f, rx * 0.93f, 5.0f, 2.5f, 1.5f, { 200, 40, 60 });
		break;
	}
	case EnemyType::Bat: {
		float fly = 32.0f + std::sin(anim_ * 0.1f) * 4.0f;
		float flap = std::sin(anim_ * 0.5f) * 0.7f;
		const Rgb body{ 120, 74, 170 }, wing{ 90, 50, 130 };
		P(Shape::Sphere, 0.0f, fly, 0.0f, radius, radius, radius, body);
		for (float x : { -6.0f, 6.0f }) P(Shape::Cone, x, fly + radius + 2.0f, 0.0f, 3.5f, 9.0f, 3.5f, body);
		for (float x : { -4.0f, 4.0f }) P(Shape::Sphere, x, fly + 2.0f, radius - 1.0f, 2.4f, 2.4f, 1.5f, { 255, 60, 60 }, 0.8f);
		for (float side : { -1.0f, 1.0f }) {
			float cx = side * (radius - 3.0f + 12.0f * std::cos(flap));
			float cy = fly + 12.0f * std::sin(flap);
			Rgb col = white ? Rgb{ 255, 255, 255 } : wing;
			r3d::Part({ pos, forward }, Shape::Box, VGet(cx * s, cy * s, 0.0f), VGet(24.0f * s, 1.5f * s, 18.0f * s), col,
				white ? 1.0f : 0.0f, r3d::Blend::Opaque, 255, VGet(0.0f, 0.0f, side * flap));
		}
		break;
	}
	case EnemyType::Mage: {
		const Rgb robe{ 70, 40, 110 }, hat{ 110, 50, 170 };
		P(Shape::Cone, 0.0f, 16.0f, 0.0f, 16.0f, 32.0f, 16.0f, robe);
		P(Shape::Sphere, 0.0f, 36.0f, 0.0f, 9.0f, 9.0f, 9.0f, { 230, 200, 170 });
		P(Shape::Cylinder, 0.0f, 43.0f, 0.0f, 14.0f, 2.0f, 14.0f, hat);
		P(Shape::Cone, 0.0f, 56.0f, 0.0f, 12.0f, 26.0f, 12.0f, hat);
		for (float x : { -3.5f, 3.5f }) P(Shape::Sphere, x, 37.0f, 8.3f, 1.8f, 1.8f, 1.0f, black);
		P(Shape::Cylinder, 16.0f, 23.0f, 4.0f, 1.6f, 46.0f, 1.6f, { 150, 100, 60 });
		const bool casting = aiTimer_ > 85;
		const float orb = casting ? 7.0f + std::sin(anim_ * 0.6f) * 2.0f : 4.0f;
		P(Shape::Sphere, 16.0f, 49.0f, 4.0f, orb, orb, orb, { 220, 120, 255 }, 0.7f);
		if (casting) {
			r3d::Part({ pos, forward }, Shape::Sphere, VGet(16.0f, 49.0f, 4.0f), VGet(orb * 2.0f, orb * 2.0f, orb * 2.0f),
				{ 200, 80, 255 }, 1.0f, r3d::Blend::Add, 110);
		}
		break;
	}
	case EnemyType::Knight:
		P(Shape::Cylinder, 0.0f, 14.0f, 0.0f, radius, 28.0f, radius, { 140, 145, 158 });
		P(Shape::Sphere, 0.0f, 36.0f, 0.0f, radius - 3.0f, radius - 3.0f, radius - 3.0f, { 180, 185, 200 });
		P(Shape::Box, 0.0f, 38.0f, radius - 5.0f, 14.0f, 3.0f, 4.0f, black);
		P(Shape::Cone, 0.0f, 58.0f, 0.0f, 4.0f, 14.0f, 4.0f, { 200, 50, 50 });
		P(Shape::Box, -(radius + 2.0f), 18.0f, 2.0f, 4.0f, 24.0f, 18.0f, { 140, 100, 50 });
		break;
	case EnemyType::Golem: {
		const Rgb rock{ 140, 125, 105 }, dark{ 110, 100, 85 };
		const float swing = std::sin(anim_ * 0.08f) * 6.0f;
		P(Shape::Ico1, 0.0f, radius * 0.95f, 0.0f, radius, radius * 0.95f, radius * 0.9f, rock);
		P(Shape::Ico, -24.0f, radius * 1.65f, 6.0f, 13.0f, 11.0f, 13.0f, dark);
		P(Shape::Ico, 22.0f, radius * 1.55f, -10.0f, 10.0f, 9.0f, 10.0f, dark);
		P(Shape::Ico, -(radius + 10.0f), 26.0f + swing, 4.0f, 17.0f, 20.0f, 17.0f, dark);
		P(Shape::Ico, radius + 10.0f, 26.0f - swing, 4.0f, 17.0f, 20.0f, 17.0f, dark);
		const Rgb eye = hp < maxHp / 2 ? Rgb{ 255, 60, 40 } : Rgb{ 255, 180, 60 };
		for (float x : { -14.0f, 14.0f }) P(Shape::Box, x, radius * 1.15f, radius * 0.82f, 12.0f, 6.0f, 4.0f, eye, 0.9f);
		break;
	}
	}
}

void Enemy::DrawOverlay(int font) const
{
	if (spawnTimer > 0) return;
	const Vec2 top = r3d::Project(pos.x, TopHeight(), pos.y);

	// 小さな HP バー（ボスは HUD に出す）
	if (!isBoss && hp < maxHp) {
		float ratio = static_cast<float>(std::max(0, hp)) / maxHp;
		float w = std::max(28.0f, radius * 2.0f);
		DrawBoxAA(top.x - w / 2, top.y - 10, top.x + w / 2, top.y - 5, GetColor(40, 20, 20), TRUE);
		DrawBoxAA(top.x - w / 2, top.y - 10, top.x - w / 2 + w * ratio, top.y - 5, GetColor(240, 70, 60), TRUE);
	}

	// 溜め中の「！」
	if ((type == EnemyType::Knight && aiState_ == 1) || (type == EnemyType::Golem && (aiState_ == 1 || aiState_ == 2))) {
		DrawStringToHandle(static_cast<int>(top.x) - 6, static_cast<int>(top.y) - 44, "！",
			GetColor(255, 80, 60), font, GetColor(40, 0, 0));
	}
}
