#include "Common.h"
#include <random>

namespace {
std::mt19937& Engine()
{
	static std::mt19937 engine{ std::random_device{}() };
	return engine;
}
}

float RandF(float lo, float hi)
{
	std::uniform_real_distribution<float> dist(lo, hi);
	return dist(Engine());
}

int RandI(int lo, int hi)
{
	std::uniform_int_distribution<int> dist(lo, hi);
	return dist(Engine());
}

bool Chance(float probability)
{
	return RandF(0.0f, 1.0f) < probability;
}

float AngleDiff(float a, float b)
{
	float d = std::fmod(a - b, kPi * 2.0f);
	if (d > kPi) d -= kPi * 2.0f;
	if (d < -kPi) d += kPi * 2.0f;
	return d;
}
