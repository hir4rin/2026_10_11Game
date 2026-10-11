#include "Enemy.h"
#include "Effect.h"
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

unsigned int Flash(bool flash, int r, int g, int b)
{
	return flash ? GetColor(255, 255, 255) : GetColor(r, g, b);
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
			world.bullets.push_back({ pos + dir * radius, dir * 4.2f, 8.0f, attack, 240, false, GetColor(200, 90, 255) });
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
				world.bullets.push_back({ pos + d * radius, d * 3.4f, 10.0f, attack * 2 / 3, 300, false, GetColor(255, 150, 60) });
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

void Enemy::Draw(const Vec2& off, int font) const
{
	const Vec2 p = pos + off;

	if (spawnTimer > 0) {
		// 出現前の魔法陣
		float t = 1.0f - static_cast<float>(spawnTimer) / (isBoss ? 90 : 45);
		SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(200 * t));
		DrawCircleAA(p.x, p.y, radius * (1.6f - t * 0.6f), 32, GetColor(200, 80, 255), FALSE, 3.0f);
		DrawCircleAA(p.x, p.y, radius * 0.6f * t, 32, GetColor(160, 60, 220), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		if (t < 0.6f) return;
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(255 * (t - 0.6f) / 0.4f));
	}

	// 影
	if (spawnTimer <= 0) SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90);
	DrawOvalAA(p.x, p.y + radius * 0.85f, radius * 0.9f, radius * 0.35f, 24, GetColor(0, 0, 0), TRUE);
	if (spawnTimer <= 0) SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	DrawBody(p, hitFlash > 4);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	// 小さな HP バー（ボスは HUD に出す）
	if (!isBoss && hp < maxHp && spawnTimer <= 0) {
		float ratio = static_cast<float>(hp) / maxHp;
		float w = radius * 2.0f;
		float y = p.y - radius - 10.0f;
		DrawBoxAA(p.x - w / 2, y, p.x + w / 2, y + 4, GetColor(40, 20, 20), TRUE);
		DrawBoxAA(p.x - w / 2, y, p.x - w / 2 + w * ratio, y + 4, GetColor(240, 70, 60), TRUE);
	}

	// 溜め中の「！」
	if ((type == EnemyType::Knight && aiState_ == 1) || (type == EnemyType::Golem && (aiState_ == 1 || aiState_ == 2))) {
		DrawStringToHandle(static_cast<int>(p.x) - 6, static_cast<int>(p.y - radius) - 36, "！",
			GetColor(255, 80, 60), font, GetColor(40, 0, 0));
	}
}

void Enemy::DrawBody(const Vec2& p, bool flash) const
{
	switch (type) {
	case EnemyType::Slime: {
		float hop = std::max(0.0f, std::sin(anim_ * 0.12f + seed_));
		float y = p.y - hop * 10.0f;
		float squash = 1.0f - hop;
		float rx = radius * (1.0f + 0.18f * squash);
		float ry = radius * (0.95f - 0.12f * squash);
		DrawOvalAA(p.x, y, rx, ry, 32, Flash(flash, 40, 110, 220), TRUE);
		DrawTriangleAA(p.x - 6, y - ry + 4, p.x + 6, y - ry + 4, p.x + 1, y - ry - 9, Flash(flash, 40, 110, 220), TRUE);
		DrawOvalAA(p.x - rx * 0.4f, y - ry * 0.4f, 4.0f, 2.5f, 12, GetColor(220, 240, 255), TRUE);
		DrawCircleAA(p.x - 5, y - 1, 2.6f, 10, GetColor(20, 20, 30), TRUE);
		DrawCircleAA(p.x + 5, y - 1, 2.6f, 10, GetColor(20, 20, 30), TRUE);
		DrawOvalAA(p.x, y + 6, 5.0f, 2.5f, 12, GetColor(200, 40, 60), TRUE);
		break;
	}
	case EnemyType::Bat: {
		float flap = std::sin(anim_ * 0.5f) * 9.0f;
		unsigned int wing = Flash(flash, 90, 50, 130);
		DrawTriangleAA(p.x - 4, p.y, p.x - 26, p.y - 8 + flap, p.x - 14, p.y + 8, wing, TRUE);
		DrawTriangleAA(p.x + 4, p.y, p.x + 26, p.y - 8 + flap, p.x + 14, p.y + 8, wing, TRUE);
		DrawCircleAA(p.x, p.y, radius, 24, Flash(flash, 120, 70, 170), TRUE);
		DrawTriangleAA(p.x - 9, p.y - 8, p.x - 4, p.y - 10, p.x - 8, p.y - 18, Flash(flash, 120, 70, 170), TRUE);
		DrawTriangleAA(p.x + 9, p.y - 8, p.x + 4, p.y - 10, p.x + 8, p.y - 18, Flash(flash, 120, 70, 170), TRUE);
		DrawCircleAA(p.x - 4, p.y - 2, 2.5f, 8, GetColor(255, 60, 60), TRUE);
		DrawCircleAA(p.x + 4, p.y - 2, 2.5f, 8, GetColor(255, 60, 60), TRUE);
		break;
	}
	case EnemyType::Mage: {
		DrawTriangleAA(p.x, p.y - 12, p.x - 16, p.y + 16, p.x + 16, p.y + 16, Flash(flash, 70, 40, 110), TRUE);
		DrawCircleAA(p.x, p.y - 6, 9.0f, 20, Flash(flash, 230, 200, 170), TRUE);
		DrawTriangleAA(p.x - 13, p.y - 10, p.x + 13, p.y - 10, p.x + 4, p.y - 34, Flash(flash, 110, 50, 170), TRUE);
		DrawCircleAA(p.x - 3, p.y - 6, 1.8f, 8, GetColor(20, 20, 30), TRUE);
		DrawCircleAA(p.x + 3, p.y - 6, 1.8f, 8, GetColor(20, 20, 30), TRUE);
		// 杖。詠唱中は光る
		DrawLineAA(p.x + 16, p.y - 16, p.x + 16, p.y + 16, GetColor(150, 100, 60), 3.0f);
		bool casting = aiTimer_ > 85;
		if (casting) SetDrawBlendMode(DX_BLENDMODE_ADD, 200);
		DrawCircleAA(p.x + 16, p.y - 18, casting ? 7.0f + std::sin(anim_ * 0.6f) * 2.0f : 4.0f, 16, GetColor(220, 120, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		break;
	}
	case EnemyType::Knight: {
		bool windup = aiState_ == 1;
		if (windup && (static_cast<int>(anim_) / 3) % 2 == 0) {
			SetDrawBlendMode(DX_BLENDMODE_ADD, 160);
			DrawCircleAA(p.x, p.y, radius + 6.0f, 32, GetColor(255, 60, 40), TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		}
		DrawCircleAA(p.x, p.y, radius, 32, Flash(flash, 90, 95, 110), TRUE);
		DrawCircleAA(p.x, p.y - 3, radius - 5.0f, 32, Flash(flash, 160, 165, 180), TRUE);
		DrawBoxAA(p.x - 10, p.y - 6, p.x + 10, p.y - 2, GetColor(20, 20, 30), TRUE);
		DrawTriangleAA(p.x - 4, p.y - radius - 2, p.x + 4, p.y - radius - 2, p.x, p.y - radius - 14, GetColor(200, 50, 50), TRUE);
		// 盾
		DrawBoxAA(p.x - radius - 6, p.y - 6, p.x - radius + 4, p.y + 12, Flash(flash, 140, 100, 50), TRUE);
		break;
	}
	case EnemyType::Golem: {
		bool windup = aiState_ == 1 || aiState_ == 2;
		if (windup) {
			SetDrawBlendMode(DX_BLENDMODE_ADD, 100 + static_cast<int>(std::sin(anim_ * 0.5f) * 60));
			DrawCircleAA(p.x, p.y, radius + 10.0f, 48, GetColor(255, 120, 40), TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		}
		DrawCircleAA(p.x, p.y, radius, 48, Flash(flash, 90, 80, 70), TRUE);
		DrawCircleAA(p.x - 4, p.y - 4, radius - 7.0f, 48, Flash(flash, 140, 125, 105), TRUE);
		DrawCircleAA(p.x - 22, p.y + 16, 9.0f, 16, Flash(flash, 110, 100, 85), TRUE);
		DrawCircleAA(p.x + 20, p.y - 22, 7.0f, 16, Flash(flash, 110, 100, 85), TRUE);
		DrawCircleAA(p.x + 26, p.y + 18, 6.0f, 16, Flash(flash, 110, 100, 85), TRUE);
		unsigned int eye = hp < maxHp / 2 ? GetColor(255, 60, 40) : GetColor(255, 180, 60);
		DrawBoxAA(p.x - 22, p.y - 12, p.x - 8, p.y - 5, eye, TRUE);
		DrawBoxAA(p.x + 8, p.y - 12, p.x + 22, p.y - 5, eye, TRUE);
		// 腕
		float swing = std::sin(anim_ * 0.08f) * 6.0f;
		DrawCircleAA(p.x - radius - 8, p.y + 10 + swing, 16.0f, 24, Flash(flash, 110, 100, 85), TRUE);
		DrawCircleAA(p.x + radius + 8, p.y + 10 - swing, 16.0f, 24, Flash(flash, 110, 100, 85), TRUE);
		break;
	}
	}
}
