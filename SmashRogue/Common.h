#pragma once
#include <cmath>

// 画面サイズ
constexpr int kScreenW = 1280;
constexpr int kScreenH = 720;

constexpr float kPi = 3.14159265358979f;

// 戦闘エリア（上部はHUD用に空けておく）
constexpr float kArenaLeft = 40.0f;
constexpr float kArenaTop = 104.0f;
constexpr float kArenaRight = 1240.0f;
constexpr float kArenaBottom = 696.0f;

struct Vec2 {
	float x = 0.0f;
	float y = 0.0f;

	constexpr Vec2() = default;
	constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

	Vec2 operator+(const Vec2& v) const { return { x + v.x, y + v.y }; }
	Vec2 operator-(const Vec2& v) const { return { x - v.x, y - v.y }; }
	Vec2 operator*(float s) const { return { x * s, y * s }; }
	Vec2 operator/(float s) const { return { x / s, y / s }; }
	Vec2 operator-() const { return { -x, -y }; }
	Vec2& operator+=(const Vec2& v) { x += v.x; y += v.y; return *this; }
	Vec2& operator-=(const Vec2& v) { x -= v.x; y -= v.y; return *this; }
	Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

	float LengthSq() const { return x * x + y * y; }
	float Length() const { return std::sqrt(LengthSq()); }
	Vec2 Normalized() const
	{
		float len = Length();
		return len > 1e-5f ? *this / len : Vec2{};
	}
	float Angle() const { return std::atan2(y, x); }
	static Vec2 FromAngle(float rad) { return { std::cos(rad), std::sin(rad) }; }
};

struct Rgb {
	int r = 255;
	int g = 255;
	int b = 255;
};

// 乱数
float RandF(float lo, float hi);
int RandI(int lo, int hi); // lo 以上 hi 以下
bool Chance(float probability);

// a - b を [-π, π] に正規化した角度差
float AngleDiff(float a, float b);
