#pragma once
#include "Common.h"
#include "DxLib.h"
#include <vector>

inline unsigned int ToColor(const Rgb& c) { return GetColor(c.r, c.g, c.b); }

// 2.5D 表示用の簡易 3D 描画。
// ゲームの (x, y) を地面の (X, -Z) に置き、高さを Y にする（DxLib は左手系）
namespace r3d {

enum class Shape { Sphere, Cone, Cylinder, Box, Ico, Ico1, Octa, Count };
enum class Blend { Opaque, Alpha, Add, Mul }; // Mul は下の色に掛け合わせて暗くする（影用）

// 地面の上での位置と向き。部品の local 座標は x=右、y=上、z=前
struct Pose {
	Vec2 pos;
	Vec2 forward{ 0.0f, 1.0f };
};

void Init();

// カメラ。FitCamera はエリアが決まったら一度呼ぶ
void FitCamera();
void FollowCamera(const Vec2& target, bool snap);
void Begin(const Vec2& shake); // 3D 描画開始（カメラ反映、Z バッファ有効）
void End();
Vec2 Project(float x, float h, float y); // 3D の点を画面座標へ

// 立体の部品
void Part(const Pose& pose, Shape shape, VECTOR local, VECTOR scale, Rgb color,
	float emissive = 0.0f, Blend blend = Blend::Opaque, int alpha = 255, VECTOR rot = VGet(0.0f, 0.0f, 0.0f));
// from から to へ伸びる角材（剣など）
void Beam(const Vec2& from, const Vec2& to, float h, float width, float height, Rgb color, float emissive = 0.0f);

// 地面に貼る平らなもの
void Shadow(const Vec2& c, float r);
void GroundDisc(const Vec2& c, float r, Rgb color, int alpha, bool add, float h = 0.6f);
void GroundRing(const Vec2& c, float r, float width, Rgb color, int alpha, bool add, float h = 1.0f);
void GroundFan(const Vec2& c, float r, float a0, float a1, Rgb color, int alpha);

// 床のように大量の四角形をまとめて描く
void PushQuad(std::vector<VERTEX3D>& out, float x0, float y0, float x1, float y1, float h, Rgb color);
void DrawTriangles(const std::vector<VERTEX3D>& verts);

}
