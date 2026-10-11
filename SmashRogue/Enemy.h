#pragma once
#include "Common.h"
#include <vector>

class EffectManager;

enum class EnemyType {
	Slime,  // 跳ねながら近づく
	Bat,    // 素早くふらふら飛ぶ
	Mage,   // 距離を取って魔法弾を撃つ
	Knight, // 溜めてから突進
	Golem,  // ボス
};

struct Projectile {
	Vec2 pos;
	Vec2 vel;
	float radius;
	int damage;
	int life;
	bool fromPlayer;
	Rgb color;
};

// ボスの地面たたきで広がる衝撃波
struct Shockwave {
	Vec2 pos;
	float radius;
	float maxRadius;
	float speed;
	int damage;
	bool hitDone;
};

// 敵が AI 更新中に触ってよい世界の情報
struct EnemyWorld {
	Vec2 playerPos;
	std::vector<Projectile>& bullets;
	std::vector<Shockwave>& shockwaves;
	EffectManager& effects;
	int summonRequest;
};

class Enemy {
public:
	Enemy(EnemyType type, const Vec2& pos, int floor);

	void Update(EnemyWorld& world);
	void Draw3D(const Vec2& playerPos) const; // 3D 描画中に呼ぶ
	void DrawOverlay(int font) const;         // HP バーなど、3D の後に重ねる

	bool IsActive() const { return alive && spawnTimer <= 0; }
	bool IsCharging() const { return type == EnemyType::Knight && aiState_ == 2; }
	const char* Name() const;

	EnemyType type;
	Vec2 pos;
	Vec2 knock;
	float radius = 16.0f;
	int hp = 1;
	int maxHp = 1;
	int attack = 1;
	float speed = 1.0f;
	int exp = 1;
	bool isBoss = false;
	bool alive = true;

	int spawnTimer = 45;
	int hitFlash = 0;
	int lastSwingHit = -1;
	int lastDashHit = -1;
	int orbHitCooldown = 0;

private:
	void UpdateGolem(EnemyWorld& world, const Vec2& dir);
	float TopHeight() const; // 頭の上の高さ（HP バーの位置）

	float anim_ = 0.0f;
	float seed_ = 0.0f;
	float strafe_ = 1.0f;
	int aiTimer_ = 0;
	int aiState_ = 0;
	int stateTimer_ = 0;
	int burstCount_ = 0;
	int summonTimer_ = 0;
	Vec2 aiDir_;
};
