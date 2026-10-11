#pragma once
#include "Common.h"
#include "Effect.h"
#include "Enemy.h"
#include "Player.h"
#include "Skill.h"
#include <vector>

class Input;

struct Fonts {
	int mini;
	int normal;
	int big;
	int huge;
};

struct RunResult {
	int floor;
	int level;
	int kills;
	int frames;
};

// ダンジョン探索のメイン部分
class PlayScene {
public:
	explicit PlayScene(const Fonts& fonts);

	void Start();
	void Update(const Input& input);
	void Draw() const;

	bool IsGameOver() const { return gameOver_; }
	bool IsQuitRequested() const { return quitRequested_; }
	RunResult GetResult() const { return { floor_, player_.Level(), kills_, frames_ }; }

private:
	enum class State { Playing, Select, Transition, Dying, Paused };

	struct Obstacle {
		Vec2 pos;
		float radius;
	};

	enum class PickupKind { Exp, Herb, Chest };
	struct Pickup {
		PickupKind kind;
		Vec2 pos;
		Vec2 vel;
		int value;
		bool magnet;
		bool taken;
		float homingSpeed = 2.0f;
	};

	// フロア進行
	void StartFloor(int floor);
	void GenerateObstacles();
	void SpawnWave();
	void SpawnEnemy(EnemyType type, const Vec2& pos);
	EnemyType PickEnemyType() const;
	Vec2 RandomFreePos(float radius, float minDistFromPlayer) const;
	void OpenStairs();

	// 更新
	void UpdatePlaying(const Input& input);
	void UpdateSelect(const Input& input);
	void UpdatePlayerAttacks();
	void UpdateSkills();
	void UpdateProjectiles();
	void UpdateShockwaves();
	void UpdateContactDamage();
	void UpdatePickups();
	void UpdateWaves();
	void ResolveCollisions();
	void PushOutOfObstacles(Vec2& pos, float radius) const;
	bool HitsObstacle(const Vec2& pos, float radius) const;

	void DamageEnemy(Enemy& enemy, int damage, const Vec2& knockDir, float knockPower, bool canCrit);
	void KillEnemy(Enemy& enemy);
	void HurtPlayer(int damage, const Vec2& from);
	void OpenSelect(bool fromChest);
	const Enemy* FindNearestEnemy(const Vec2& from, float maxDist) const;

	// 描画
	void DrawFloor(const Vec2& off) const;
	void DrawPickups(const Vec2& off) const;
	void DrawHud() const;
	void DrawSelect() const;
	void DrawCentered(const char* text, int y, unsigned int color, int font) const;

	Fonts fonts_;
	State state_ = State::Playing;
	Player player_;
	std::vector<Enemy> enemies_;
	std::vector<Projectile> bullets_;
	std::vector<Shockwave> shockwaves_;
	std::vector<Pickup> pickups_;
	std::vector<Obstacle> obstacles_;
	EffectManager effects_;

	int floor_ = 1;
	int wave_ = 0;
	int totalWaves_ = 0;
	int waveDelay_ = 0;
	bool bossFloor_ = false;
	bool stairsOpen_ = false;
	Vec2 stairsPos_;
	int bannerTimer_ = 0;

	int kills_ = 0;
	int frames_ = 0;
	int hitStop_ = 0;
	int stateTimer_ = 0;
	int fireballTimer_ = 0;
	int thunderTimer_ = 0;

	// レベルアップ / 宝箱の選択
	int pendingLevelUps_ = 0;
	int pendingChests_ = 0;
	bool selectFromChest_ = false;
	std::vector<SkillId> choices_;
	int cursor_ = 0;

	bool gameOver_ = false;
	bool quitRequested_ = false;
};
