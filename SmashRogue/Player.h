#pragma once
#include "Common.h"
#include "Skill.h"

class Input;

class Player {
public:
	static constexpr float kRadius = 18.0f;
	static constexpr int kSwingFrames = 12;    // 振りモーション全体
	static constexpr int kSwingHitFrames = 7;  // そのうち当たり判定がある時間
	static constexpr int kDashFrames = 9;

	void Reset();                          // 新しい冒険の開始
	void PlaceForFloor(const Vec2& start); // フロア開始時
	void Update(const Input& input, const Vec2* aimTarget);
	void Draw(const Vec2& off) const;

	// 能力値（スキルレベルから算出）
	int MaxHp() const;
	int AttackPower() const;
	float MoveSpeed() const;
	float AttackRange() const;
	float AttackArc() const;
	int AttackCooldown() const;
	float CritRate() const;
	float PickupRadius() const;
	int DashCooldown() const;

	int SkillLevel(SkillId id) const { return skillLevels_[static_cast<int>(id)]; }
	bool IsSkillMax(SkillId id) const { return SkillLevel(id) >= GetSkillInfo(id).maxLevel; }
	void LevelUpSkill(SkillId id);

	int Hp() const { return hp_; }
	int Level() const { return level_; }
	int Exp() const { return exp_; }
	int ExpToNext() const { return 5 + level_ * 5; }
	int AddExp(int amount); // 戻り値: 上がったレベル数
	bool TakeDamage(int damage, const Vec2& from);
	void Heal(int amount);
	bool IsDead() const { return hp_ <= 0; }
	void Knockback(const Vec2& v) { knock_ += v; }

	// 剣
	bool IsSwingHitActive() const { return swingTimer_ > kSwingFrames - kSwingHitFrames; }
	int SwingId() const { return swingId_; }
	float SwingAngle() const { return swingAngle_; }
	bool IsSpinSwing() const { return swingSpin_; }

	// ダッシュ
	bool IsDashing() const { return dashTimer_ > 0; }
	int DashId() const { return dashId_; }

	// ガードオーブ
	int OrbCount() const { return SkillLevel(SkillId::Orb); }
	Vec2 OrbPos(int index) const;
	static constexpr float kOrbRadius = 11.0f;

	Vec2 pos;

private:
	int skillLevels_[kSkillCount] = {};
	int hp_ = 0;
	int level_ = 1;
	int exp_ = 0;

	Vec2 facing_{ 1.0f, 0.0f };
	Vec2 knock_;

	int attackCooldown_ = 0;
	int swingTimer_ = 0;
	int swingId_ = 0;
	float swingAngle_ = 0.0f;
	bool swingSpin_ = false;

	int dashTimer_ = 0;
	int dashCooldown_ = 0;
	int dashId_ = 0;
	Vec2 dashDir_;
	Vec2 trail_[6];
	int trailCount_ = 0;

	int invincible_ = 0;
	int frame_ = 0;
	float orbAngle_ = 0.0f;
};
