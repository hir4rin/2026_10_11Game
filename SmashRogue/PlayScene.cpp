#include "PlayScene.h"
#include "Input.h"
#include "DxLib.h"
#include <algorithm>
#include <cstring>
#include <string>

namespace {
constexpr int kTransitionFrames = 60;
constexpr int kDyingFrames = 120;
constexpr int kSelectInputDelay = 25;

const Vec2 kArenaCenter((kArenaLeft + kArenaRight) * 0.5f, (kArenaTop + kArenaBottom) * 0.5f);

struct FloorTheme {
	int a[3];
	int b[3];
	int wall[3];
};
const FloorTheme kThemes[] = {
	{ { 72, 64, 56 }, { 80, 71, 62 }, { 40, 34, 30 } },   // 土の洞窟
	{ { 52, 70, 60 }, { 58, 78, 66 }, { 26, 40, 34 } },   // 苔むした遺跡
	{ { 56, 60, 84 }, { 62, 66, 92 }, { 28, 30, 48 } },   // 氷の回廊
	{ { 84, 52, 48 }, { 92, 58, 52 }, { 46, 24, 22 } },   // 溶岩の間
	{ { 66, 52, 80 }, { 72, 58, 88 }, { 36, 26, 46 } },   // 魔の城
};

unsigned int ThemeColor(const int c[3])
{
	return GetColor(c[0], c[1], c[2]);
}
}

PlayScene::PlayScene(const Fonts& fonts)
	: fonts_(fonts)
{
}

void PlayScene::Start()
{
	player_.Reset();
	kills_ = 0;
	frames_ = 0;
	pendingLevelUps_ = 0;
	pendingChests_ = 0;
	fireballTimer_ = 0;
	thunderTimer_ = 0;
	gameOver_ = false;
	quitRequested_ = false;
	StartFloor(1);
}

// ---------------------------------------------------------------------------
// フロア進行
// ---------------------------------------------------------------------------

void PlayScene::StartFloor(int floor)
{
	floor_ = floor;
	enemies_.clear();
	bullets_.clear();
	shockwaves_.clear();
	pickups_.clear();
	effects_.Clear();

	player_.PlaceForFloor(kArenaCenter);
	bossFloor_ = (floor % 5 == 0);
	GenerateObstacles();

	wave_ = 0;
	totalWaves_ = bossFloor_ ? 1 : std::min(2 + floor / 3, 5);
	waveDelay_ = 70;
	stairsOpen_ = false;
	bannerTimer_ = 150;
	hitStop_ = 0;
	state_ = State::Playing;
}

void PlayScene::GenerateObstacles()
{
	obstacles_.clear();
	const int count = bossFloor_ ? RandI(2, 3) : RandI(3, 7);
	for (int tries = 0; tries < 200 && static_cast<int>(obstacles_.size()) < count; ++tries) {
		float r = RandF(24.0f, 46.0f);
		Vec2 p(RandF(kArenaLeft + 90.0f, kArenaRight - 90.0f), RandF(kArenaTop + 80.0f, kArenaBottom - 80.0f));
		if ((p - kArenaCenter).Length() < 170.0f + r) continue;
		bool overlap = false;
		for (const auto& o : obstacles_) {
			if ((o.pos - p).Length() < o.radius + r + 90.0f) overlap = true;
		}
		if (!overlap) obstacles_.push_back({ p, r });
	}
}

Vec2 PlayScene::RandomFreePos(float radius, float minDistFromPlayer) const
{
	Vec2 p;
	for (int tries = 0; tries < 100; ++tries) {
		p = Vec2(RandF(kArenaLeft + radius + 10.0f, kArenaRight - radius - 10.0f),
			RandF(kArenaTop + radius + 10.0f, kArenaBottom - radius - 10.0f));
		if ((p - player_.pos).Length() < minDistFromPlayer) continue;
		if (HitsObstacle(p, radius + 10.0f)) continue;
		return p;
	}
	return p;
}

EnemyType PlayScene::PickEnemyType() const
{
	struct Entry {
		EnemyType type;
		int weight;
	};
	const Entry table[] = {
		{ EnemyType::Slime, 10 },
		{ EnemyType::Bat, floor_ >= 2 ? 7 : 0 },
		{ EnemyType::Mage, floor_ >= 3 ? 4 + floor_ / 3 : 0 },
		{ EnemyType::Knight, floor_ >= 4 ? 3 + floor_ / 2 : 0 },
	};
	int total = 0;
	for (const auto& e : table) total += e.weight;
	int r = RandI(0, total - 1);
	for (const auto& e : table) {
		if (r < e.weight) return e.type;
		r -= e.weight;
	}
	return EnemyType::Slime;
}

void PlayScene::SpawnEnemy(EnemyType type, const Vec2& pos)
{
	enemies_.emplace_back(type, pos, floor_);
}

void PlayScene::SpawnWave()
{
	++wave_;
	if (bossFloor_) {
		SpawnEnemy(EnemyType::Golem, Vec2(kArenaCenter.x, kArenaTop + 140.0f));
		for (int i = 0; i < 3 + floor_ / 5; ++i) SpawnEnemy(PickEnemyType(), RandomFreePos(20.0f, 250.0f));
		effects_.Shake(8.0f);
		return;
	}
	const int count = std::min(5 + floor_ * 2 + wave_ * 2, 30);
	for (int i = 0; i < count; ++i) SpawnEnemy(PickEnemyType(), RandomFreePos(20.0f, 230.0f));
}

void PlayScene::OpenStairs()
{
	stairsOpen_ = true;
	stairsPos_ = RandomFreePos(40.0f, 200.0f);
	effects_.Ring(stairsPos_, 120.0f, GetColor(255, 230, 120), 40);
	effects_.Text(player_.pos + Vec2(0.0f, -50.0f), "かいだんが あらわれた！", GetColor(255, 230, 120), true);

	// 残った経験値はすべて吸い寄せる
	for (auto& p : pickups_) {
		if (p.kind == PickupKind::Exp) p.magnet = true;
	}
	// 宝箱（ボス階は確定）
	if (bossFloor_ || Chance(0.4f)) {
		pickups_.push_back({ PickupKind::Chest, RandomFreePos(30.0f, 150.0f), {}, 0, false, false });
	}
}

// ---------------------------------------------------------------------------
// 更新
// ---------------------------------------------------------------------------

void PlayScene::Update(const Input& input)
{
	switch (state_) {
	case State::Playing:
		if (input.IsTrigger(Button::Pause)) {
			state_ = State::Paused;
			return;
		}
		UpdatePlaying(input);
		break;
	case State::Select:
		UpdateSelect(input);
		break;
	case State::Paused:
		if (input.IsTrigger(Button::Pause)) state_ = State::Playing;
		if (input.IsTrigger(Button::Quit)) quitRequested_ = true;
		break;
	case State::Transition:
		++stateTimer_;
		if (stateTimer_ == kTransitionFrames / 2) {
			StartFloor(floor_ + 1);
			state_ = State::Transition; // StartFloor が Playing に戻すので維持する
		}
		if (stateTimer_ >= kTransitionFrames) state_ = State::Playing;
		effects_.Update();
		break;
	case State::Dying:
		++stateTimer_;
		effects_.Update();
		if (stateTimer_ >= kDyingFrames) gameOver_ = true;
		break;
	}
}

void PlayScene::UpdatePlaying(const Input& input)
{
	if (bannerTimer_ > 0) --bannerTimer_;

	// ヒットストップ中は時間を止める
	if (hitStop_ > 0) {
		--hitStop_;
		return;
	}
	++frames_;

	const Enemy* aim = FindNearestEnemy(player_.pos, player_.AttackRange() * 1.8f);
	const Vec2 aimPos = aim != nullptr ? aim->pos : Vec2{};
	player_.Update(input, aim != nullptr ? &aimPos : nullptr);

	EnemyWorld world{ player_.pos, bullets_, shockwaves_, effects_, 0 };
	for (auto& e : enemies_) e.Update(world);
	for (int i = 0; i < world.summonRequest; ++i) {
		SpawnEnemy(Chance(0.5f) ? EnemyType::Bat : EnemyType::Slime, RandomFreePos(20.0f, 200.0f));
	}

	UpdatePlayerAttacks();
	UpdateSkills();
	UpdateProjectiles();
	UpdateShockwaves();
	UpdateContactDamage();
	ResolveCollisions();

	enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& e) { return !e.alive; }), enemies_.end());

	UpdatePickups();
	UpdateWaves();
	effects_.Update();

	if (player_.IsDead()) {
		state_ = State::Dying;
		stateTimer_ = 0;
		effects_.Burst(player_.pos, GetColor(80, 140, 255), 40, 9.0f, 7.0f, 60);
		effects_.Shake(14.0f);
		return;
	}

	if (stairsOpen_ && (player_.pos - stairsPos_).Length() < 30.0f) {
		state_ = State::Transition;
		stateTimer_ = 0;
		return;
	}

	if (pendingChests_ > 0) OpenSelect(true);
	else if (pendingLevelUps_ > 0) OpenSelect(false);
}

const Enemy* PlayScene::FindNearestEnemy(const Vec2& from, float maxDist) const
{
	const Enemy* best = nullptr;
	float bestDist = maxDist;
	for (const auto& e : enemies_) {
		if (!e.IsActive()) continue;
		float d = (e.pos - from).Length() - e.radius;
		if (d < bestDist) {
			bestDist = d;
			best = &e;
		}
	}
	return best;
}

void PlayScene::UpdatePlayerAttacks()
{
	// 剣
	if (player_.IsSwingHitActive()) {
		const bool spin = player_.IsSpinSwing();
		const float range = player_.AttackRange();
		const float halfArc = player_.AttackArc() * 0.5f;
		bool hitAny = false;
		for (auto& e : enemies_) {
			if (!e.IsActive() || e.lastSwingHit == player_.SwingId()) continue;
			Vec2 d = e.pos - player_.pos;
			float dist = d.Length();
			if (dist > range + e.radius) continue;
			if (!spin && dist > e.radius) {
				float tolerance = std::asin(std::min(1.0f, e.radius / dist));
				if (std::fabs(AngleDiff(d.Angle(), player_.SwingAngle())) > halfArc + tolerance) continue;
			}
			e.lastSwingHit = player_.SwingId();
			int damage = spin ? player_.AttackPower() * 13 / 10 : player_.AttackPower();
			DamageEnemy(e, damage, d.Normalized(), 9.0f, true);
			hitAny = true;
		}
		if (hitAny) {
			hitStop_ = std::max(hitStop_, 3);
			effects_.Shake(3.0f);
		}
	}

	// ダッシュ体当たり
	const int dashLv = player_.SkillLevel(SkillId::DashUp);
	if (dashLv > 0 && player_.IsDashing()) {
		for (auto& e : enemies_) {
			if (!e.IsActive() || e.lastDashHit == player_.DashId()) continue;
			Vec2 d = e.pos - player_.pos;
			if (d.Length() > Player::kRadius + e.radius + 6.0f) continue;
			e.lastDashHit = player_.DashId();
			DamageEnemy(e, player_.AttackPower() * (4 + dashLv * 4) / 10, d.Normalized(), 12.0f, false);
		}
	}

	// ガードオーブ
	const int orbLv = player_.SkillLevel(SkillId::Orb);
	for (int i = 0; i < player_.OrbCount(); ++i) {
		const Vec2 o = player_.OrbPos(i);
		for (auto& e : enemies_) {
			if (!e.IsActive() || e.orbHitCooldown > 0) continue;
			Vec2 d = e.pos - o;
			if (d.Length() > Player::kOrbRadius + e.radius) continue;
			e.orbHitCooldown = 24;
			DamageEnemy(e, 5 + orbLv * 3 + player_.AttackPower() / 4, d.Normalized(), 5.0f, false);
		}
	}
}

void PlayScene::UpdateSkills()
{
	// ファイアボール
	const int fireLv = player_.SkillLevel(SkillId::Fireball);
	if (fireLv > 0 && ++fireballTimer_ >= 100 - fireLv * 10) {
		const Enemy* target = FindNearestEnemy(player_.pos, 520.0f);
		if (target != nullptr) {
			fireballTimer_ = 0;
			const int shots = 1 + (fireLv - 1) / 2;
			const float baseAngle = (target->pos - player_.pos).Angle();
			for (int i = 0; i < shots; ++i) {
				float angle = baseAngle + (i - (shots - 1) * 0.5f) * 0.25f;
				bullets_.push_back({ player_.pos, Vec2::FromAngle(angle) * 8.0f, 10.0f,
					8 + fireLv * 5 + player_.AttackPower() / 3, 120, true, GetColor(255, 130, 40) });
			}
		}
	}

	// いかずち
	const int thunderLv = player_.SkillLevel(SkillId::Thunder);
	if (thunderLv > 0 && ++thunderTimer_ >= 210 - thunderLv * 25) {
		std::vector<Enemy*> candidates;
		for (auto& e : enemies_) {
			if (e.IsActive()) candidates.push_back(&e);
		}
		if (!candidates.empty()) {
			thunderTimer_ = 0;
			const int strikes = 1 + thunderLv / 3;
			for (int s = 0; s < strikes; ++s) {
				const Vec2 at = candidates[RandI(0, static_cast<int>(candidates.size()) - 1)]->pos;
				effects_.Lightning(at);
				effects_.Shake(5.0f);
				for (auto& e : enemies_) {
					Vec2 d = e.pos - at;
					if (e.IsActive() && d.Length() < 70.0f + e.radius) {
						DamageEnemy(e, 18 + thunderLv * 10, d.Normalized(), 4.0f, false);
					}
				}
			}
		}
	}
}

void PlayScene::UpdateProjectiles()
{
	for (auto& b : bullets_) {
		b.pos += b.vel;
		--b.life;
		if (b.pos.x < kArenaLeft || b.pos.x > kArenaRight || b.pos.y < kArenaTop || b.pos.y > kArenaBottom
			|| HitsObstacle(b.pos, b.radius)) {
			b.life = 0;
			effects_.Burst(b.pos, b.color, 5, 3.0f, 4.0f, 14);
			continue;
		}
		if (b.fromPlayer) {
			for (auto& e : enemies_) {
				if (!e.IsActive() || (e.pos - b.pos).Length() > e.radius + b.radius) continue;
				DamageEnemy(e, b.damage, b.vel.Normalized(), 5.0f, true);
				effects_.Burst(b.pos, b.color, 10, 5.0f, 5.0f, 18);
				b.life = 0;
				break;
			}
		} else if ((player_.pos - b.pos).Length() < Player::kRadius + b.radius - 3.0f) {
			HurtPlayer(b.damage, b.pos);
			b.life = 0;
		}
	}
	bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Projectile& b) { return b.life <= 0; }), bullets_.end());
}

void PlayScene::UpdateShockwaves()
{
	for (auto& w : shockwaves_) {
		w.radius += w.speed;
		float dist = (player_.pos - w.pos).Length();
		if (!w.hitDone && std::fabs(dist - w.radius) < 14.0f + Player::kRadius) {
			w.hitDone = true;
			HurtPlayer(w.damage, w.pos);
		}
	}
	shockwaves_.erase(std::remove_if(shockwaves_.begin(), shockwaves_.end(),
		[](const Shockwave& w) { return w.radius >= w.maxRadius; }), shockwaves_.end());
}

void PlayScene::UpdateContactDamage()
{
	for (const auto& e : enemies_) {
		if (!e.IsActive()) continue;
		if ((e.pos - player_.pos).Length() < e.radius + Player::kRadius - 4.0f) {
			HurtPlayer(e.IsCharging() ? e.attack * 3 / 2 : e.attack, e.pos);
		}
	}
}

void PlayScene::HurtPlayer(int damage, const Vec2& from)
{
	if (!player_.TakeDamage(damage, from)) return;
	effects_.Text(player_.pos + Vec2(0.0f, -30.0f), std::to_string(damage), GetColor(255, 80, 80), true);
	effects_.Burst(player_.pos, GetColor(255, 80, 80), 12, 6.0f, 5.0f, 24);
	effects_.Shake(8.0f);
	hitStop_ = std::max(hitStop_, 5);
}

void PlayScene::ResolveCollisions()
{
	// 敵同士の押し出し
	for (size_t i = 0; i < enemies_.size(); ++i) {
		for (size_t j = i + 1; j < enemies_.size(); ++j) {
			Enemy& a = enemies_[i];
			Enemy& b = enemies_[j];
			if (!a.IsActive() || !b.IsActive()) continue;
			Vec2 d = b.pos - a.pos;
			float dist = d.Length();
			float minDist = a.radius + b.radius;
			if (dist >= minDist) continue;
			Vec2 n = dist > 0.01f ? d / dist : Vec2(1.0f, 0.0f);
			float push = (minDist - dist) * 0.5f;
			// ボスは押されにくい
			float wa = a.isBoss ? 0.1f : (b.isBoss ? 1.9f : 1.0f);
			float wb = 2.0f - wa;
			a.pos -= n * push * wa;
			b.pos += n * push * wb;
		}
	}
	for (auto& e : enemies_) PushOutOfObstacles(e.pos, e.radius);
	PushOutOfObstacles(player_.pos, Player::kRadius);
}

bool PlayScene::HitsObstacle(const Vec2& pos, float radius) const
{
	for (const auto& o : obstacles_) {
		if ((pos - o.pos).Length() < o.radius + radius) return true;
	}
	return false;
}

void PlayScene::PushOutOfObstacles(Vec2& pos, float radius) const
{
	for (const auto& o : obstacles_) {
		Vec2 d = pos - o.pos;
		float dist = d.Length();
		float minDist = o.radius + radius;
		if (dist < minDist) {
			Vec2 n = dist > 0.01f ? d / dist : Vec2(1.0f, 0.0f);
			pos = o.pos + n * minDist;
		}
	}
	pos.x = std::clamp(pos.x, kArenaLeft + radius, kArenaRight - radius);
	pos.y = std::clamp(pos.y, kArenaTop + radius, kArenaBottom - radius);
}

void PlayScene::DamageEnemy(Enemy& enemy, int damage, const Vec2& knockDir, float knockPower, bool canCrit)
{
	if (!enemy.IsActive()) return;
	const bool crit = canCrit && Chance(player_.CritRate());
	int dmg = crit ? damage * 2 : damage;
	dmg = std::max(1, static_cast<int>(dmg * RandF(0.9f, 1.1f)));

	enemy.hp -= dmg;
	enemy.hitFlash = 8;
	enemy.knock += knockDir * knockPower * (enemy.isBoss ? 0.1f : 1.0f);

	const Vec2 textPos = enemy.pos + Vec2(0.0f, -enemy.radius - 14.0f);
	if (crit) {
		effects_.Text(textPos, std::to_string(dmg) + "!", GetColor(255, 220, 60), true);
		effects_.Burst(enemy.pos, GetColor(255, 220, 80), 12, 8.0f, 5.0f, 22);
		hitStop_ = std::max(hitStop_, 5);
	} else {
		effects_.Text(textPos, std::to_string(dmg), GetColor(255, 255, 255));
		effects_.Burst(enemy.pos, GetColor(255, 255, 220), 5, 5.0f, 4.0f, 16);
	}

	if (enemy.hp <= 0) KillEnemy(enemy);
}

void PlayScene::KillEnemy(Enemy& enemy)
{
	enemy.alive = false;
	++kills_;

	effects_.Burst(enemy.pos, GetColor(255, 255, 255), enemy.isBoss ? 60 : 14, enemy.isBoss ? 12.0f : 6.0f, 6.0f, 30);

	// 経験値の宝石をばらまく（1個あたり最大5）
	int remain = enemy.exp;
	while (remain > 0) {
		int v = std::min(5, remain);
		remain -= v;
		Vec2 vel = Vec2::FromAngle(RandF(0.0f, kPi * 2.0f)) * RandF(1.0f, 4.0f);
		pickups_.push_back({ PickupKind::Exp, enemy.pos, vel, v, false, false });
	}
	if (Chance(enemy.isBoss ? 1.0f : 0.05f)) {
		pickups_.push_back({ PickupKind::Herb, enemy.pos, Vec2::FromAngle(RandF(0.0f, kPi * 2.0f)) * 2.0f, 0, false, false });
	}

	const int drain = player_.SkillLevel(SkillId::Drain);
	if (drain > 0) player_.Heal(drain * 2);

	if (enemy.isBoss) {
		effects_.Shake(20.0f);
		effects_.Ring(enemy.pos, 300.0f, GetColor(255, 200, 100), 50);
		effects_.Text(enemy.pos + Vec2(0.0f, -80.0f), "ボスを たおした！", GetColor(255, 220, 100), true);
		hitStop_ = 30;
		// 手下も一緒に消える
		for (auto& e : enemies_) {
			if (e.alive && !e.isBoss) {
				e.alive = false;
				effects_.Burst(e.pos, GetColor(200, 120, 255), 10, 5.0f, 5.0f, 24);
			}
		}
		bullets_.clear();
		shockwaves_.clear();
	}
}

void PlayScene::UpdatePickups()
{
	const float pickupRadius = player_.PickupRadius();
	for (auto& p : pickups_) {
		Vec2 d = player_.pos - p.pos;
		float dist = d.Length();

		if (p.kind != PickupKind::Chest && dist < pickupRadius) p.magnet = true;
		if (p.magnet) {
			// まっすぐ追いかけて徐々に加速（加速度で引くと周回してしまう）
			p.homingSpeed = std::min(16.0f, p.homingSpeed + 0.6f);
			p.vel = d.Normalized() * std::min(p.homingSpeed, dist);
		} else {
			p.vel *= 0.88f;
		}
		p.pos += p.vel;

		if (dist < Player::kRadius + 10.0f) {
			p.taken = true;
			switch (p.kind) {
			case PickupKind::Exp: {
				int ups = player_.AddExp(p.value);
				if (ups > 0) {
					pendingLevelUps_ += ups;
					effects_.Text(player_.pos + Vec2(0.0f, -46.0f), "レベルアップ！", GetColor(120, 255, 160), true);
					effects_.Ring(player_.pos, 90.0f, GetColor(120, 255, 160), 30);
				}
				break;
			}
			case PickupKind::Herb: {
				int amount = player_.MaxHp() / 4;
				player_.Heal(amount);
				effects_.Text(player_.pos + Vec2(0.0f, -46.0f), "+" + std::to_string(amount), GetColor(100, 255, 120), true);
				effects_.Burst(player_.pos, GetColor(100, 255, 120), 14, 5.0f, 4.0f, 26);
				break;
			}
			case PickupKind::Chest:
				++pendingChests_;
				effects_.Burst(p.pos, GetColor(255, 220, 80), 30, 8.0f, 6.0f, 36);
				effects_.Ring(p.pos, 100.0f, GetColor(255, 220, 80), 30);
				break;
			}
		}
	}
	pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(), [](const Pickup& p) { return p.taken; }), pickups_.end());
}

void PlayScene::UpdateWaves()
{
	if (stairsOpen_) return;
	if (!enemies_.empty()) return;

	if (wave_ < totalWaves_) {
		if (--waveDelay_ <= 0) {
			SpawnWave();
			waveDelay_ = 50;
		}
	} else {
		OpenStairs();
	}
}

// ---------------------------------------------------------------------------
// レベルアップ / 宝箱
// ---------------------------------------------------------------------------

void PlayScene::OpenSelect(bool fromChest)
{
	if (fromChest) --pendingChests_;
	else --pendingLevelUps_;
	selectFromChest_ = fromChest;

	std::vector<SkillId> pool;
	for (int i = 0; i < kSkillCount; ++i) {
		SkillId id = static_cast<SkillId>(i);
		if (!player_.IsSkillMax(id)) pool.push_back(id);
	}
	choices_.clear();
	while (!pool.empty() && choices_.size() < 3) {
		int idx = RandI(0, static_cast<int>(pool.size()) - 1);
		choices_.push_back(pool[idx]);
		pool.erase(pool.begin() + idx);
	}
	// 全部最大なら回復のみ（SkillId::Count を「やくそう」として使う）
	if (choices_.empty()) choices_.push_back(SkillId::Count);

	cursor_ = 0;
	stateTimer_ = 0;
	state_ = State::Select;
}

void PlayScene::UpdateSelect(const Input& input)
{
	effects_.Update();
	// 攻撃ボタン押しっぱなしで誤選択しないよう少し待つ
	if (++stateTimer_ < kSelectInputDelay) return;

	const int count = static_cast<int>(choices_.size());
	if (input.IsTrigger(Button::Left) || input.IsTrigger(Button::Up)) cursor_ = (cursor_ + count - 1) % count;
	if (input.IsTrigger(Button::Right) || input.IsTrigger(Button::Down)) cursor_ = (cursor_ + 1) % count;

	if (input.IsTrigger(Button::Confirm)) {
		SkillId id = choices_[cursor_];
		if (id == SkillId::Count) {
			player_.Heal(player_.MaxHp());
		} else {
			player_.LevelUpSkill(id);
			const SkillInfo& info = GetSkillInfo(id);
			effects_.Ring(player_.pos, 100.0f, GetColor(info.r, info.g, info.b), 30);
		}
		state_ = State::Playing;
		player_.Heal(player_.MaxHp() / 10); // レベルアップのおまけ回復
	}
}

// ---------------------------------------------------------------------------
// 描画
// ---------------------------------------------------------------------------

void PlayScene::Draw() const
{
	const Vec2 off = effects_.ShakeOffset();

	DrawFloor(off);
	DrawPickups(off);

	for (const auto& w : shockwaves_) {
		float t = w.radius / w.maxRadius;
		SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(230 * (1.0f - t)));
		DrawCircleAA(w.pos.x + off.x, w.pos.y + off.y, w.radius, 64, GetColor(255, 160, 80), FALSE, 10.0f);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	}

	// y 座標順に並べて奥から描く
	std::vector<const Enemy*> sorted;
	for (const auto& e : enemies_) sorted.push_back(&e);
	std::sort(sorted.begin(), sorted.end(), [](const Enemy* a, const Enemy* b) { return a->pos.y < b->pos.y; });
	bool playerDrawn = state_ == State::Dying;
	for (const Enemy* e : sorted) {
		if (!playerDrawn && e->pos.y > player_.pos.y) {
			player_.Draw(off);
			playerDrawn = true;
		}
		e->Draw(off, fonts_.normal);
	}
	if (!playerDrawn) player_.Draw(off);

	for (const auto& b : bullets_) {
		SetDrawBlendMode(DX_BLENDMODE_ADD, 160);
		DrawCircleAA(b.pos.x + off.x, b.pos.y + off.y, b.radius + 5.0f, 16, b.color, TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawCircleAA(b.pos.x + off.x, b.pos.y + off.y, b.radius * 0.6f, 16, GetColor(255, 255, 230), TRUE);
	}

	effects_.Draw(off, fonts_.mini, fonts_.normal);
	DrawHud();

	// フロア開始の表示
	if (bannerTimer_ > 0 && state_ != State::Transition) {
		int alpha = std::min(255, bannerTimer_ * 6);
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
		std::string text = "B" + std::to_string(floor_) + "F";
		DrawCentered(text.c_str(), 250, GetColor(255, 255, 255), fonts_.huge);
		if (bossFloor_) DrawCentered("― ボスの気配がする… ―", 340, GetColor(255, 120, 100), fonts_.big);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	}

	switch (state_) {
	case State::Select:
		DrawSelect();
		break;
	case State::Paused:
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
		DrawBox(0, 0, kScreenW, kScreenH, GetColor(0, 0, 0), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawCentered("ポーズ", 260, GetColor(255, 255, 255), fonts_.huge);
		DrawCentered("Esc / P : さいかい      Q : タイトルへ", 380, GetColor(220, 220, 220), fonts_.normal);
		break;
	case State::Transition: {
		float t = static_cast<float>(stateTimer_) / kTransitionFrames;
		float a = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(255 * a));
		DrawBox(0, 0, kScreenW, kScreenH, GetColor(0, 0, 0), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		break;
	}
	case State::Dying: {
		float t = static_cast<float>(stateTimer_) / kDyingFrames;
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(200 * t));
		DrawBox(0, 0, kScreenW, kScreenH, GetColor(60, 0, 0), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		break;
	}
	default:
		break;
	}
}

void PlayScene::DrawFloor(const Vec2& off) const
{
	const FloorTheme& theme = kThemes[((floor_ - 1) / 2) % (sizeof(kThemes) / sizeof(kThemes[0]))];
	DrawBox(0, 0, kScreenW, kScreenH, GetColor(14, 12, 18), TRUE);

	// 壁
	const int wallW = 14;
	DrawBox(static_cast<int>(kArenaLeft + off.x) - wallW, static_cast<int>(kArenaTop + off.y) - wallW,
		static_cast<int>(kArenaRight + off.x) + wallW, static_cast<int>(kArenaBottom + off.y) + wallW, ThemeColor(theme.wall), TRUE);

	// 床のタイル
	const int tile = 48;
	const unsigned int ca = ThemeColor(theme.a);
	const unsigned int cb = ThemeColor(theme.b);
	for (int y = static_cast<int>(kArenaTop); y < kArenaBottom; y += tile) {
		for (int x = static_cast<int>(kArenaLeft); x < kArenaRight; x += tile) {
			int tx = (x - static_cast<int>(kArenaLeft)) / tile;
			int ty = (y - static_cast<int>(kArenaTop)) / tile;
			int x2 = std::min(x + tile, static_cast<int>(kArenaRight));
			int y2 = std::min(y + tile, static_cast<int>(kArenaBottom));
			DrawBox(x + static_cast<int>(off.x), y + static_cast<int>(off.y), x2 + static_cast<int>(off.x), y2 + static_cast<int>(off.y),
				(tx + ty) % 2 == 0 ? ca : cb, TRUE);
		}
	}

	// 階段
	if (stairsOpen_) {
		const Vec2 s = stairsPos_ + off;
		float pulse = (std::sin(frames_ * 0.1f) + 1.0f) * 0.5f;
		SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(80 + 80 * pulse));
		DrawCircleAA(s.x, s.y, 46.0f + pulse * 6.0f, 32, GetColor(255, 220, 120), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawBoxAA(s.x - 28, s.y - 28, s.x + 28, s.y + 28, GetColor(20, 16, 14), TRUE);
		for (int i = 0; i < 4; ++i) {
			float y = s.y - 24 + i * 13.0f;
			DrawBoxAA(s.x - 24 + i * 4.0f, y, s.x + 24 - i * 4.0f, y + 6, GetColor(110 - i * 20, 100 - i * 20, 90 - i * 18), TRUE);
		}
		DrawStringToHandle(static_cast<int>(s.x) - 40, static_cast<int>(s.y) - 56, "▼ つぎへ", GetColor(255, 230, 140), fonts_.mini, GetColor(30, 20, 0));
	}

	// 岩
	for (const auto& o : obstacles_) {
		const Vec2 p = o.pos + off;
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 100);
		DrawOvalAA(p.x + 6, p.y + o.radius * 0.6f, o.radius, o.radius * 0.45f, 32, GetColor(0, 0, 0), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawCircleAA(p.x, p.y, o.radius, 40, GetColor(70, 66, 72), TRUE);
		DrawCircleAA(p.x - o.radius * 0.15f, p.y - o.radius * 0.15f, o.radius * 0.8f, 40, GetColor(110, 106, 112), TRUE);
		DrawCircleAA(p.x - o.radius * 0.35f, p.y - o.radius * 0.35f, o.radius * 0.25f, 20, GetColor(150, 146, 152), TRUE);
	}
}

void PlayScene::DrawPickups(const Vec2& off) const
{
	for (const auto& p : pickups_) {
		const Vec2 q = p.pos + off;
		switch (p.kind) {
		case PickupKind::Exp: {
			float s = 4.0f + p.value;
			unsigned int col = p.value >= 5 ? GetColor(120, 255, 140) : GetColor(100, 200, 255);
			SetDrawBlendMode(DX_BLENDMODE_ADD, 110);
			DrawCircleAA(q.x, q.y, s + 4.0f, 12, col, TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
			DrawTriangleAA(q.x, q.y - s, q.x + s * 0.7f, q.y, q.x - s * 0.7f, q.y, col, TRUE);
			DrawTriangleAA(q.x, q.y + s, q.x + s * 0.7f, q.y, q.x - s * 0.7f, q.y, col, TRUE);
			break;
		}
		case PickupKind::Herb:
			DrawOvalAA(q.x - 5, q.y, 6.0f, 10.0f, 16, GetColor(60, 200, 80), TRUE);
			DrawOvalAA(q.x + 5, q.y, 6.0f, 10.0f, 16, GetColor(80, 230, 100), TRUE);
			DrawLineAA(q.x, q.y + 4, q.x, q.y + 14, GetColor(60, 140, 50), 2.0f);
			break;
		case PickupKind::Chest: {
			float bob = std::sin(frames_ * 0.08f) * 3.0f;
			SetDrawBlendMode(DX_BLENDMODE_ADD, 90);
			DrawCircleAA(q.x, q.y, 34.0f, 24, GetColor(255, 220, 80), TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
			DrawBoxAA(q.x - 20, q.y - 12 + bob, q.x + 20, q.y + 14 + bob, GetColor(150, 80, 30), TRUE);
			DrawBoxAA(q.x - 20, q.y - 16 + bob, q.x + 20, q.y - 4 + bob, GetColor(190, 110, 40), TRUE);
			DrawBoxAA(q.x - 20, q.y - 5 + bob, q.x + 20, q.y - 2 + bob, GetColor(240, 200, 60), TRUE);
			DrawBoxAA(q.x - 4, q.y - 8 + bob, q.x + 4, q.y + 4 + bob, GetColor(250, 220, 80), TRUE);
			break;
		}
		}
	}
}

void PlayScene::DrawHud() const
{
	DrawBox(0, 0, kScreenW, 84, GetColor(18, 16, 24), TRUE);
	DrawLine(0, 84, kScreenW, 84, GetColor(70, 60, 90));

	// HP
	const int barX = 20;
	const int barW = 320;
	float hpRatio = static_cast<float>(player_.Hp()) / player_.MaxHp();
	unsigned int hpCol = hpRatio > 0.5f ? GetColor(80, 220, 100) : (hpRatio > 0.25f ? GetColor(240, 200, 60) : GetColor(240, 70, 60));
	DrawBox(barX, 14, barX + barW, 38, GetColor(40, 30, 40), TRUE);
	DrawBox(barX, 14, barX + static_cast<int>(barW * hpRatio), 38, hpCol, TRUE);
	DrawBox(barX, 14, barX + barW, 38, GetColor(200, 200, 210), FALSE);
	DrawFormatStringToHandle(barX + 8, 16, GetColor(255, 255, 255), fonts_.mini, "HP %d / %d", player_.Hp(), player_.MaxHp());

	// EXP
	float expRatio = static_cast<float>(player_.Exp()) / player_.ExpToNext();
	DrawBox(barX, 46, barX + barW, 58, GetColor(30, 30, 50), TRUE);
	DrawBox(barX, 46, barX + static_cast<int>(barW * expRatio), 58, GetColor(90, 170, 255), TRUE);
	DrawBox(barX, 46, barX + barW, 58, GetColor(150, 150, 180), FALSE);
	DrawFormatStringToHandle(barX, 62, GetColor(200, 210, 255), fonts_.mini, "Lv %d   こうげき %d", player_.Level(), player_.AttackPower());

	// フロア情報
	DrawFormatStringToHandle(370, 10, GetColor(255, 230, 140), fonts_.big, "B%dF", floor_);
	if (stairsOpen_) {
		DrawStringToHandle(370, 54, "かいだんへ すすめ！", GetColor(255, 230, 140), fonts_.mini);
	} else if (bossFloor_) {
		DrawStringToHandle(370, 54, "BOSS", GetColor(255, 100, 90), fonts_.mini);
	} else {
		DrawFormatStringToHandle(370, 54, GetColor(220, 220, 220), fonts_.mini, "WAVE %d / %d", std::max(1, wave_), totalWaves_);
	}
	DrawFormatStringToHandle(530, 18, GetColor(220, 220, 220), fonts_.mini, "たおした数 %d", kills_);
	DrawFormatStringToHandle(530, 44, GetColor(220, 220, 220), fonts_.mini, "じかん %d:%02d", frames_ / 3600, frames_ / 60 % 60);

	// 取得スキル
	int slot = 0;
	for (int i = 0; i < kSkillCount; ++i) {
		int lv = player_.SkillLevel(static_cast<SkillId>(i));
		if (lv == 0) continue;
		const SkillInfo& info = GetSkillInfo(static_cast<SkillId>(i));
		int col = slot % 4;
		int row = slot / 4;
		int x = 680 + col * 146;
		int y = 6 + row * 25;
		DrawBox(x, y, x + 140, y + 22, GetColor(info.r / 4, info.g / 4, info.b / 4), TRUE);
		DrawBox(x, y, x + 4, y + 22, GetColor(info.r, info.g, info.b), TRUE);
		DrawFormatStringToHandle(x + 8, y + 3, GetColor(info.r, info.g, info.b), fonts_.mini, "%s %d", info.name, lv);
		++slot;
	}

	// ボスの HP
	for (const auto& e : enemies_) {
		if (!e.isBoss || !e.alive) continue;
		const int w = 700;
		const int x = (kScreenW - w) / 2;
		const int y = kScreenH - 40;
		float ratio = static_cast<float>(std::max(0, e.hp)) / e.maxHp;
		DrawBox(x - 2, y - 2, x + w + 2, y + 18, GetColor(10, 10, 10), TRUE);
		DrawBox(x, y, x + static_cast<int>(w * ratio), y + 16, GetColor(220, 60, 50), TRUE);
		DrawStringToHandle(x, y - 26, e.Name(), GetColor(255, 200, 180), fonts_.mini, GetColor(0, 0, 0));
	}
}

void PlayScene::DrawSelect() const
{
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
	DrawBox(0, 0, kScreenW, kScreenH, GetColor(0, 0, 10), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	DrawCentered(selectFromChest_ ? "たからばこ を あけた！" : "レベルアップ！", 130,
		selectFromChest_ ? GetColor(255, 220, 80) : GetColor(120, 255, 160), fonts_.huge);
	DrawCentered("スキルを ひとつ えらんでください", 215, GetColor(220, 220, 230), fonts_.normal);

	const int count = static_cast<int>(choices_.size());
	const int cardW = 320;
	const int cardH = 220;
	const int gap = 30;
	const int totalW = count * cardW + (count - 1) * gap;
	const int startX = (kScreenW - totalW) / 2;
	const int y = 270;

	for (int i = 0; i < count; ++i) {
		const int x = startX + i * (cardW + gap);
		const bool selected = i == cursor_;
		const int lift = selected ? -10 : 0;

		SkillId id = choices_[i];
		const char* name = "やくそう";
		const char* desc = "HPを ぜんかいふく する";
		int r = 100, g = 255, b = 120;
		int lv = 0;
		int maxLv = 0;
		if (id != SkillId::Count) {
			const SkillInfo& info = GetSkillInfo(id);
			name = info.name;
			desc = info.desc;
			r = info.r;
			g = info.g;
			b = info.b;
			lv = player_.SkillLevel(id);
			maxLv = info.maxLevel;
		}

		DrawBox(x, y + lift, x + cardW, y + cardH + lift, selected ? GetColor(40, 40, 60) : GetColor(26, 26, 38), TRUE);
		DrawBox(x, y + lift, x + cardW, y + 8 + lift, GetColor(r, g, b), TRUE);
		DrawBox(x, y + lift, x + cardW, y + cardH + lift, selected ? GetColor(255, 255, 255) : GetColor(90, 90, 110), FALSE);
		if (selected) DrawBox(x - 2, y - 2 + lift, x + cardW + 2, y + cardH + 2 + lift, GetColor(255, 255, 255), FALSE);

		DrawStringToHandle(x + 20, y + 30 + lift, name, GetColor(r, g, b), fonts_.big, GetColor(0, 0, 0));
		if (id != SkillId::Count) {
			if (lv == 0) DrawStringToHandle(x + 20, y + 90 + lift, "NEW!", GetColor(255, 220, 80), fonts_.normal);
			else DrawFormatStringToHandle(x + 20, y + 90 + lift, GetColor(230, 230, 230), fonts_.normal, "Lv %d → %d", lv, lv + 1);
			// レベルの目盛り
			for (int k = 0; k < maxLv; ++k) {
				int bx = x + 20 + k * 22;
				DrawBox(bx, y + 126 + lift, bx + 16, y + 136 + lift, k < lv + 1 ? GetColor(r, g, b) : GetColor(60, 60, 70), TRUE);
			}
		}
		DrawStringToHandle(x + 20, y + 160 + lift, desc, GetColor(210, 210, 220), fonts_.mini);
	}

	if (stateTimer_ >= kSelectInputDelay) {
		DrawCentered("← → でえらぶ   Z / Enter でけってい", 540, GetColor(200, 200, 210), fonts_.normal);
	}
}

void PlayScene::DrawCentered(const char* text, int y, unsigned int color, int font) const
{
	int width = GetDrawStringWidthToHandle(text, static_cast<int>(std::strlen(text)), font);
	DrawStringToHandle((kScreenW - width) / 2, y, text, color, font, GetColor(0, 0, 0));
}
