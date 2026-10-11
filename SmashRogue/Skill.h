#pragma once

// レベルアップ時に選べるスキル
enum class SkillId {
	Power,      // ちから
	Vitality,   // たいりょく
	Speed,      // すばやさ
	Reach,      // リーチ
	Rapid,      // れんげき
	Critical,   // かいしん
	Drain,      // きゅうけつ
	Fireball,   // ファイアボール
	Thunder,    // いかずち
	Orb,        // ガードオーブ
	Spin,       // かいてんぎり
	DashUp,     // ダッシュ強化
	Magnet,     // マグネット
	Count
};

constexpr int kSkillCount = static_cast<int>(SkillId::Count);

struct SkillInfo {
	const char* name;
	const char* desc;
	int maxLevel;
	int r, g, b; // テーマカラー
};

const SkillInfo& GetSkillInfo(SkillId id);
