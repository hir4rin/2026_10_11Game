#include "Player.h"
#include "Input.h"
#include "DxLib.h"
#include <algorithm>

namespace {
// 扇形を三角形の集まりで描く
void DrawFan(const Vec2& c, float radius, float a0, float a1, unsigned int color)
{
	const int steps = std::max(4, static_cast<int>(std::fabs(a1 - a0) / 0.12f));
	for (int i = 0; i < steps; ++i) {
		float t0 = a0 + (a1 - a0) * i / steps;
		float t1 = a0 + (a1 - a0) * (i + 1) / steps;
		Vec2 p0 = c + Vec2::FromAngle(t0) * radius;
		Vec2 p1 = c + Vec2::FromAngle(t1) * radius;
		DrawTriangleAA(c.x, c.y, p0.x, p0.y, p1.x, p1.y, color, TRUE);
	}
}
}

void Player::Reset()
{
	for (int& lv : skillLevels_) lv = 0;
	level_ = 1;
	exp_ = 0;
	hp_ = MaxHp();
	swingId_ = 0;
	dashId_ = 0;
	PlaceForFloor({ (kArenaLeft + kArenaRight) * 0.5f, (kArenaTop + kArenaBottom) * 0.5f });
}

void Player::PlaceForFloor(const Vec2& start)
{
	pos = start;
	facing_ = { 1.0f, 0.0f };
	knock_ = {};
	attackCooldown_ = 0;
	swingTimer_ = 0;
	dashTimer_ = 0;
	dashCooldown_ = 0;
	trailCount_ = 0;
	invincible_ = 60;
}

int Player::MaxHp() const { return 100 + 25 * SkillLevel(SkillId::Vitality); }
int Player::AttackPower() const { return 10 + 4 * SkillLevel(SkillId::Power) + (level_ - 1); }
float Player::MoveSpeed() const { return 3.6f + 0.45f * SkillLevel(SkillId::Speed); }
float Player::AttackRange() const { return 78.0f + 16.0f * SkillLevel(SkillId::Reach); }
float Player::AttackArc() const { return 2.2f + 0.1f * SkillLevel(SkillId::Reach); }
int Player::AttackCooldown() const { return 24 - 4 * SkillLevel(SkillId::Rapid); }
float Player::CritRate() const { return 0.05f + 0.08f * SkillLevel(SkillId::Critical); }
float Player::PickupRadius() const { return 70.0f + 60.0f * SkillLevel(SkillId::Magnet); }
int Player::DashCooldown() const { return 70 - 15 * SkillLevel(SkillId::DashUp); }

void Player::LevelUpSkill(SkillId id)
{
	if (IsSkillMax(id)) return;
	++skillLevels_[static_cast<int>(id)];
	if (id == SkillId::Vitality) Heal(25);
}

int Player::AddExp(int amount)
{
	exp_ += amount;
	int ups = 0;
	while (exp_ >= ExpToNext()) {
		exp_ -= ExpToNext();
		++level_;
		++ups;
	}
	return ups;
}

bool Player::TakeDamage(int damage, const Vec2& from)
{
	if (invincible_ > 0 || dashTimer_ > 0 || hp_ <= 0) return false;
	hp_ = std::max(0, hp_ - damage);
	invincible_ = 50;
	knock_ += (pos - from).Normalized() * 7.0f;
	return true;
}

void Player::Heal(int amount)
{
	hp_ = std::min(MaxHp(), hp_ + amount);
}

Vec2 Player::OrbPos(int index) const
{
	float angle = orbAngle_ + kPi * 2.0f * index / std::max(1, OrbCount());
	return pos + Vec2::FromAngle(angle) * 78.0f;
}

void Player::Update(const Input& input, const Vec2* aimTarget)
{
	++frame_;
	if (attackCooldown_ > 0) --attackCooldown_;
	if (swingTimer_ > 0) --swingTimer_;
	if (dashCooldown_ > 0) --dashCooldown_;
	if (invincible_ > 0) --invincible_;
	orbAngle_ += 0.065f;

	const Vec2 move = input.MoveAxis();

	if (dashTimer_ > 0) {
		--dashTimer_;
		pos += dashDir_ * 13.0f;
		// 残像
		for (int i = 5; i > 0; --i) trail_[i] = trail_[i - 1];
		trail_[0] = pos;
		trailCount_ = std::min(6, trailCount_ + 1);
	} else {
		trailCount_ = std::max(0, trailCount_ - 1);
		float speed = MoveSpeed() * (swingTimer_ > 0 ? 0.55f : 1.0f);
		pos += move * speed;
		if (move.LengthSq() > 0.01f && swingTimer_ == 0) facing_ = move.Normalized();

		if (input.IsTrigger(Button::Dash) && dashCooldown_ == 0) {
			dashDir_ = move.LengthSq() > 0.01f ? move.Normalized() : facing_;
			facing_ = dashDir_;
			dashTimer_ = kDashFrames;
			dashCooldown_ = DashCooldown();
			++dashId_;
			trail_[0] = pos;
			trailCount_ = 1;
		}
	}

	// 押しっぱなしで連続攻撃。近くの敵へ自動で向く
	if (input.IsPress(Button::Attack) && attackCooldown_ == 0 && dashTimer_ == 0) {
		if (aimTarget != nullptr) {
			Vec2 toTarget = *aimTarget - pos;
			if (toTarget.LengthSq() > 1.0f) facing_ = toTarget.Normalized();
		}
		swingAngle_ = facing_.Angle();
		swingSpin_ = Chance(0.15f * SkillLevel(SkillId::Spin));
		swingTimer_ = kSwingFrames;
		attackCooldown_ = AttackCooldown();
		++swingId_;
	}

	pos += knock_;
	knock_ *= 0.78f;
}

void Player::Draw(const Vec2& off) const
{
	const Vec2 p = pos + off;

	// 影
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90);
	DrawOvalAA(p.x, p.y + kRadius * 0.85f, kRadius * 0.9f, kRadius * 0.35f, 24, GetColor(0, 0, 0), TRUE);

	// ダッシュの残像
	for (int i = 1; i < trailCount_; ++i) {
		SetDrawBlendMode(DX_BLENDMODE_ADD, 120 - i * 18);
		const Vec2 t = trail_[i] + off;
		DrawCircleAA(t.x, t.y, kRadius, 24, GetColor(80, 160, 255), TRUE);
	}

	// 剣の軌跡
	if (swingTimer_ > 0) {
		float t = 1.0f - static_cast<float>(swingTimer_) / kSwingFrames;
		float ease = 1.0f - (1.0f - t) * (1.0f - t);
		float arc = swingSpin_ ? kPi * 2.0f : AttackArc();
		float start = swingAngle_ - arc * 0.5f;
		float current = start + arc * std::min(1.0f, ease * 1.25f);
		unsigned int col = swingSpin_ ? GetColor(200, 140, 255) : GetColor(255, 255, 255);

		SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(140 * (1.0f - t)));
		DrawFan(p, AttackRange(), start, current, col);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

		Vec2 dir = Vec2::FromAngle(current);
		Vec2 a = p + dir * kRadius;
		Vec2 b = p + dir * (AttackRange() * 0.92f);
		DrawLineAA(a.x, a.y, b.x, b.y, GetColor(220, 230, 240), 6.0f);
		DrawLineAA(a.x, a.y, b.x, b.y, GetColor(255, 255, 255), 2.0f);
	}

	// 本体（無敵中は点滅）
	bool blink = invincible_ > 0 && dashTimer_ == 0 && (frame_ / 3) % 2 == 0;
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, blink ? 90 : 255);
	DrawCircleAA(p.x, p.y, kRadius, 32, GetColor(30, 60, 140), TRUE);
	DrawCircleAA(p.x, p.y, kRadius - 2.5f, 32, GetColor(70, 130, 240), TRUE);
	// 頭のツンツン
	Vec2 tuft = p + Vec2(0.0f, -kRadius + 2.0f);
	DrawTriangleAA(tuft.x - 7, tuft.y + 2, tuft.x + 7, tuft.y + 2, tuft.x + 3, tuft.y - 10, GetColor(250, 210, 70), TRUE);
	// 目（向いている方向へずらす）
	Vec2 side(-facing_.y, facing_.x);
	Vec2 eye = p + facing_ * 7.0f;
	Vec2 e1 = eye + side * 5.0f;
	Vec2 e2 = eye - side * 5.0f;
	DrawCircleAA(e1.x, e1.y, 3.2f, 12, GetColor(255, 255, 255), TRUE);
	DrawCircleAA(e2.x, e2.y, 3.2f, 12, GetColor(255, 255, 255), TRUE);
	DrawCircleAA(e1.x + facing_.x, e1.y + facing_.y, 1.8f, 8, GetColor(20, 20, 40), TRUE);
	DrawCircleAA(e2.x + facing_.x, e2.y + facing_.y, 1.8f, 8, GetColor(20, 20, 40), TRUE);

	// 構えている剣
	if (swingTimer_ == 0) {
		Vec2 hand = p + side * (kRadius - 2.0f);
		Vec2 tip = hand + facing_ * 22.0f + side * 4.0f;
		DrawLineAA(hand.x, hand.y, tip.x, tip.y, GetColor(220, 230, 240), 4.0f);
	}
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	// ガードオーブ
	for (int i = 0; i < OrbCount(); ++i) {
		Vec2 o = OrbPos(i) + off;
		SetDrawBlendMode(DX_BLENDMODE_ADD, 120);
		DrawCircleAA(o.x, o.y, kOrbRadius + 6.0f, 20, GetColor(80, 140, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawCircleAA(o.x, o.y, kOrbRadius, 20, GetColor(190, 230, 255), TRUE);
	}

	// ダッシュのクールダウン
	if (dashCooldown_ > 0) {
		float ratio = 1.0f - static_cast<float>(dashCooldown_) / DashCooldown();
		DrawBoxAA(p.x - 18, p.y + 26, p.x + 18, p.y + 30, GetColor(30, 30, 40), TRUE);
		DrawBoxAA(p.x - 18, p.y + 26, p.x - 18 + 36 * ratio, p.y + 30, GetColor(100, 255, 180), TRUE);
	}
}
