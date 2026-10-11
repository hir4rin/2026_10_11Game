#include "Player.h"
#include "Input.h"
#include "Render3D.h"
#include "DxLib.h"
#include <algorithm>

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

void Player::Draw3D() const
{
	using r3d::Shape;
	const bool blink = invincible_ > 0 && dashTimer_ == 0 && (frame_ / 3) % 2 == 0;
	const Vec2 side(-facing_.y, facing_.x);

	r3d::Shadow(pos, kRadius * 0.95f);

	// ダッシュの残像
	for (int i = 1; i < trailCount_; ++i) {
		r3d::Part({ trail_[i], facing_ }, Shape::Sphere, VGet(0.0f, kRadius + 2.0f, 0.0f), VGet(kRadius, kRadius, kRadius),
			{ 80, 160, 255 }, 1.0f, r3d::Blend::Add, 110 - i * 16);
	}

	// 剣。振っている間は軌跡の扇を地面に出す
	if (swingTimer_ > 0) {
		float t = 1.0f - static_cast<float>(swingTimer_) / kSwingFrames;
		float ease = 1.0f - (1.0f - t) * (1.0f - t);
		float arc = swingSpin_ ? kPi * 2.0f : AttackArc();
		float start = swingAngle_ - arc * 0.5f;
		float current = start + arc * std::min(1.0f, ease * 1.25f);
		r3d::GroundFan(pos, AttackRange(), start, current, swingSpin_ ? Rgb{ 200, 140, 255 } : Rgb{ 255, 255, 255 },
			static_cast<int>(130 * (1.0f - t)));
		Vec2 dir = Vec2::FromAngle(current);
		if (!blink) {
			r3d::Beam(pos + dir * kRadius, pos + dir * (AttackRange() * 0.92f), 22.0f, 4.0f, 1.6f, { 220, 230, 240 }, 0.3f);
			r3d::Part({ pos + dir * kRadius, dir }, Shape::Box, VGet(0.0f, 22.0f, 0.0f), VGet(12.0f, 3.0f, 3.0f), { 140, 90, 42 });
		}
	} else if (!blink) {
		Vec2 hand = pos + side * (kRadius - 2.0f);
		r3d::Beam(hand, hand + facing_ * 26.0f, 22.0f, 3.0f, 1.6f, { 220, 230, 240 }, 0.3f);
	}

	if (!blink) {
		const r3d::Pose pose{ pos, facing_ };
		r3d::Part(pose, Shape::Sphere, VGet(0.0f, kRadius + 2.0f, 0.0f), VGet(kRadius, kRadius * 1.05f, kRadius), { 70, 130, 240 });
		r3d::Part(pose, Shape::Cone, VGet(0.0f, kRadius * 2.0f + 7.0f, -2.0f), VGet(6.0f, 12.0f, 6.0f), { 250, 210, 70 });
		for (float s : { -1.0f, 1.0f }) {
			r3d::Part(pose, Shape::Sphere, VGet(s * 6.0f, kRadius + 7.0f, kRadius - 2.2f), VGet(3.6f, 3.8f, 2.5f), { 255, 255, 255 }, 0.4f);
			r3d::Part(pose, Shape::Sphere, VGet(s * 6.0f, kRadius + 7.0f, kRadius + 0.2f), VGet(2.0f, 2.2f, 1.4f), { 20, 20, 40 });
		}
	}

	// ガードオーブ
	for (int i = 0; i < OrbCount(); ++i) {
		const Vec2 o = OrbPos(i);
		r3d::Shadow(o, kOrbRadius * 0.8f);
		r3d::Part({ o, facing_ }, Shape::Sphere, VGet(0.0f, 24.0f, 0.0f), VGet(kOrbRadius, kOrbRadius, kOrbRadius), { 190, 230, 255 }, 0.6f);
		r3d::Part({ o, facing_ }, Shape::Sphere, VGet(0.0f, 24.0f, 0.0f), VGet(kOrbRadius + 6.0f, kOrbRadius + 6.0f, kOrbRadius + 6.0f),
			{ 80, 140, 255 }, 1.0f, r3d::Blend::Add, 90);
	}
}

void Player::DrawOverlay() const
{
	// ダッシュのクールダウン
	if (dashCooldown_ > 0) {
		const Vec2 p = r3d::Project(pos.x, 0.0f, pos.y);
		float ratio = 1.0f - static_cast<float>(dashCooldown_) / DashCooldown();
		DrawBoxAA(p.x - 18, p.y + 10, p.x + 18, p.y + 14, GetColor(30, 30, 40), TRUE);
		DrawBoxAA(p.x - 18, p.y + 10, p.x - 18 + 36 * ratio, p.y + 14, GetColor(100, 255, 180), TRUE);
	}
}
