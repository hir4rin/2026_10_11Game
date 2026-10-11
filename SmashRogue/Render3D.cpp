#include "Render3D.h"
#include <algorithm>
#include <array>

namespace r3d {
namespace {

struct Mesh {
	std::vector<VECTOR> pos;
	std::vector<VECTOR> norm;
	std::vector<unsigned short> idx;

	void Add(VECTOR p, VECTOR n)
	{
		pos.push_back(p);
		norm.push_back(n);
	}
	void Tri(int a, int b, int c)
	{
		idx.push_back(static_cast<unsigned short>(a));
		idx.push_back(static_cast<unsigned short>(b));
		idx.push_back(static_cast<unsigned short>(c));
	}
	// 面法線で三角形を足す（角ばった見た目用）
	void FlatTri(VECTOR a, VECTOR b, VECTOR c)
	{
		VECTOR n = VNorm(VCross(VSub(b, a), VSub(c, a)));
		if (VDot(n, VAdd(VAdd(a, b), c)) < 0.0f) n = VScale(n, -1.0f); // 外向きにそろえる
		const int base = static_cast<int>(pos.size());
		Add(a, n);
		Add(b, n);
		Add(c, n);
		Tri(base, base + 1, base + 2);
	}
};

Mesh g_mesh[static_cast<int>(Shape::Count)];
std::vector<VERTEX3D> g_buf;
std::vector<VERTEX3D> g_ground;
VECTOR g_light;

constexpr float kFov = 34.0f * kPi / 180.0f;
constexpr float kHudBottom = 84.0f;
constexpr float kScreenCenterY = (kHudBottom + kScreenH) * 0.5f;
constexpr float kPitch = 0.98f; // 見下ろし角

struct Camera {
	float cx = 0.0f, cz = 0.0f; // エリア中央
	float d = 1000.0f;          // 注視点からの距離
	float k = 1.0f;             // 全体表示に対する寄り具合
	float tx = 0.0f, tz = 0.0f; // 注視点
} g_cam;

VECTOR ToDx(float x, float h, float y) { return VGet(x, h, -y); }

Mesh MakeSphere(int lat, int lon)
{
	Mesh m;
	for (int i = 0; i <= lat; ++i) {
		float phi = kPi * i / lat;
		for (int j = 0; j <= lon; ++j) {
			float th = kPi * 2.0f * j / lon;
			VECTOR p = VGet(std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th));
			m.Add(p, p);
		}
	}
	for (int i = 0; i < lat; ++i) {
		for (int j = 0; j < lon; ++j) {
			int a = i * (lon + 1) + j, b = a + lon + 1;
			m.Tri(a, b, a + 1);
			m.Tri(a + 1, b, b + 1);
		}
	}
	return m;
}

Mesh MakeCone(int seg)
{
	Mesh m;
	const float s = 1.0f / std::sqrt(2.0f);
	for (int j = 0; j < seg; ++j) {
		float a0 = kPi * 2.0f * j / seg, a1 = kPi * 2.0f * (j + 1) / seg, am = (a0 + a1) * 0.5f;
		int base = static_cast<int>(m.pos.size());
		m.Add(VGet(0.0f, 0.5f, 0.0f), VGet(std::cos(am) * s, s, std::sin(am) * s));
		m.Add(VGet(std::cos(a0), -0.5f, std::sin(a0)), VGet(std::cos(a0) * s, s, std::sin(a0) * s));
		m.Add(VGet(std::cos(a1), -0.5f, std::sin(a1)), VGet(std::cos(a1) * s, s, std::sin(a1) * s));
		m.Tri(base, base + 1, base + 2);
		// 底面
		base = static_cast<int>(m.pos.size());
		m.Add(VGet(0.0f, -0.5f, 0.0f), VGet(0.0f, -1.0f, 0.0f));
		m.Add(VGet(std::cos(a0), -0.5f, std::sin(a0)), VGet(0.0f, -1.0f, 0.0f));
		m.Add(VGet(std::cos(a1), -0.5f, std::sin(a1)), VGet(0.0f, -1.0f, 0.0f));
		m.Tri(base, base + 2, base + 1);
	}
	return m;
}

Mesh MakeCylinder(int seg)
{
	Mesh m;
	for (int j = 0; j < seg; ++j) {
		float a0 = kPi * 2.0f * j / seg, a1 = kPi * 2.0f * (j + 1) / seg;
		VECTOR n0 = VGet(std::cos(a0), 0.0f, std::sin(a0)), n1 = VGet(std::cos(a1), 0.0f, std::sin(a1));
		int base = static_cast<int>(m.pos.size());
		m.Add(VGet(n0.x, -0.5f, n0.z), n0);
		m.Add(VGet(n1.x, -0.5f, n1.z), n1);
		m.Add(VGet(n0.x, 0.5f, n0.z), n0);
		m.Add(VGet(n1.x, 0.5f, n1.z), n1);
		m.Tri(base, base + 2, base + 1);
		m.Tri(base + 1, base + 2, base + 3);
		for (float y : { -0.5f, 0.5f }) {
			VECTOR n = VGet(0.0f, y * 2.0f, 0.0f);
			base = static_cast<int>(m.pos.size());
			m.Add(VGet(0.0f, y, 0.0f), n);
			m.Add(VGet(n0.x, y, n0.z), n);
			m.Add(VGet(n1.x, y, n1.z), n);
			m.Tri(base, base + 1, base + 2);
		}
	}
	return m;
}

Mesh MakeBox()
{
	Mesh m;
	const VECTOR dirs[6] = { VGet(1, 0, 0), VGet(-1, 0, 0), VGet(0, 1, 0), VGet(0, -1, 0), VGet(0, 0, 1), VGet(0, 0, -1) };
	for (const VECTOR& n : dirs) {
		// 面の2軸
		VECTOR u = std::fabs(n.y) > 0.5f ? VGet(1, 0, 0) : VGet(0, 1, 0);
		VECTOR v = VCross(n, u);
		int base = static_cast<int>(m.pos.size());
		for (int k = 0; k < 4; ++k) {
			float su = (k == 0 || k == 3) ? -0.5f : 0.5f;
			float sv = (k < 2) ? -0.5f : 0.5f;
			m.Add(VAdd(VScale(n, 0.5f), VAdd(VScale(u, su), VScale(v, sv))), n);
		}
		m.Tri(base, base + 1, base + 2);
		m.Tri(base, base + 2, base + 3);
	}
	return m;
}

// 正二十面体（subdiv 回だけ分割して球に近づける）。面ごとに角ばらせる
Mesh MakeIco(int subdiv)
{
	const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
	std::vector<VECTOR> v = {
		VGet(-1, t, 0), VGet(1, t, 0), VGet(-1, -t, 0), VGet(1, -t, 0),
		VGet(0, -1, t), VGet(0, 1, t), VGet(0, -1, -t), VGet(0, 1, -t),
		VGet(t, 0, -1), VGet(t, 0, 1), VGet(-t, 0, -1), VGet(-t, 0, 1),
	};
	for (auto& p : v) p = VNorm(p);
	std::vector<std::array<VECTOR, 3>> faces;
	const int f[20][3] = {
		{ 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
		{ 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
		{ 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
		{ 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 },
	};
	for (const auto& tri : f) faces.push_back({ v[tri[0]], v[tri[1]], v[tri[2]] });
	for (int s = 0; s < subdiv; ++s) {
		std::vector<std::array<VECTOR, 3>> next;
		for (const auto& fc : faces) {
			VECTOR a = VNorm(VAdd(fc[0], fc[1])), b = VNorm(VAdd(fc[1], fc[2])), c = VNorm(VAdd(fc[2], fc[0]));
			next.push_back({ fc[0], a, c });
			next.push_back({ fc[1], b, a });
			next.push_back({ fc[2], c, b });
			next.push_back({ a, b, c });
		}
		faces = next;
	}
	Mesh m;
	for (const auto& fc : faces) m.FlatTri(fc[0], fc[1], fc[2]);
	return m;
}

Mesh MakeOcta()
{
	const VECTOR px = VGet(1, 0, 0), nx = VGet(-1, 0, 0), py = VGet(0, 1, 0), ny = VGet(0, -1, 0), pz = VGet(0, 0, 1), nz = VGet(0, 0, -1);
	Mesh m;
	for (const VECTOR& y : { py, ny }) {
		m.FlatTri(y, px, pz);
		m.FlatTri(y, pz, nx);
		m.FlatTri(y, nx, nz);
		m.FlatTri(y, nz, px);
	}
	return m;
}

COLOR_U8 Lit(const Rgb& c, VECTOR n, float emissive, int alpha)
{
	float diff = std::max(0.0f, VDot(n, g_light));
	float amb = 0.42f + 0.16f * n.y;
	float l = std::min(1.3f, amb + 0.72f * diff);
	float k = (1.0f - emissive) * l + emissive;
	return GetColorU8(std::min(255, static_cast<int>(c.r * k)), std::min(255, static_cast<int>(c.g * k)),
		std::min(255, static_cast<int>(c.b * k)), alpha);
}

void SetBlend(Blend blend, int alpha)
{
	switch (blend) {
	case Blend::Opaque:
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetWriteZBuffer3D(TRUE);
		break;
	case Blend::Alpha:
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
		SetWriteZBuffer3D(FALSE);
		break;
	case Blend::Add:
		SetDrawBlendMode(DX_BLENDMODE_ADD, alpha);
		SetWriteZBuffer3D(FALSE);
		break;
	case Blend::Mul:
		SetDrawBlendMode(DX_BLENDMODE_MUL, alpha);
		SetWriteZBuffer3D(FALSE);
		break;
	}
}

void ResetBlend()
{
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetWriteZBuffer3D(TRUE);
}

VERTEX3D GroundVertex(float x, float y, float h, COLOR_U8 c)
{
	VERTEX3D v{};
	v.pos = ToDx(x, h, y);
	v.norm = VGet(0.0f, 1.0f, 0.0f);
	v.dif = c;
	v.spc = GetColorU8(0, 0, 0, 0);
	return v;
}

void DrawGround(Blend blend, int alpha)
{
	if (g_ground.empty()) return;
	SetBlend(blend, alpha);
	DrawPolygon3D(g_ground.data(), static_cast<int>(g_ground.size() / 3), DX_NONE_GRAPH, blend != Blend::Opaque);
	ResetBlend();
	g_ground.clear();
}

void ApplyCamera(float tx, float tz)
{
	SetupCamera_Perspective(kFov);
	SetCameraNearFar(40.0f, 6000.0f);
	SetCameraScreenCenter(kScreenW * 0.5f, kScreenCenterY);
	VECTOR target = ToDx(tx, 0.0f, tz);
	VECTOR eye = ToDx(tx, std::sin(kPitch) * g_cam.d, tz + std::cos(kPitch) * g_cam.d);
	SetCameraPositionAndTarget_UpVecY(eye, target);
}

} // namespace

void Init()
{
	g_mesh[static_cast<int>(Shape::Sphere)] = MakeSphere(12, 18);
	g_mesh[static_cast<int>(Shape::Cone)] = MakeCone(18);
	g_mesh[static_cast<int>(Shape::Cylinder)] = MakeCylinder(16);
	g_mesh[static_cast<int>(Shape::Box)] = MakeBox();
	g_mesh[static_cast<int>(Shape::Ico)] = MakeIco(0);
	g_mesh[static_cast<int>(Shape::Ico1)] = MakeIco(1);
	g_mesh[static_cast<int>(Shape::Octa)] = MakeOcta();
	// 光は左奥の上から
	g_light = VNorm(VGet(-0.4f, 1.0f, 0.45f));
}

void FitCamera()
{
	g_cam.cx = (kArenaLeft + kArenaRight) * 0.5f;
	g_cam.cz = (kArenaTop + kArenaBottom) * 0.5f;

	// エリアの四隅（壁の上端も含む）が HUD の下に収まる距離を二分探索
	auto fits = [](float d) {
		g_cam.d = d;
		ApplyCamera(g_cam.cx, g_cam.cz);
		for (float x : { kArenaLeft - 14.0f, kArenaRight + 14.0f }) {
			for (float y : { kArenaTop - 14.0f, kArenaBottom + 14.0f }) {
				for (float h : { 0.0f, 34.0f }) {
					VECTOR s = ConvWorldPosToScreenPos(ToDx(x, h, y));
					if (s.z < 0.0f || s.z > 1.0f) return false;
					if (s.x < 10.0f || s.x > kScreenW - 10.0f || s.y < kHudBottom + 6.0f || s.y > kScreenH - 6.0f) return false;
				}
			}
		}
		return true;
	};
	float lo = 100.0f, hi = 9000.0f;
	for (int i = 0; i < 40; ++i) {
		float mid = (lo + hi) * 0.5f;
		if (fits(mid)) hi = mid;
		else lo = mid;
	}
	// 全体だとキャラが小さいので、寄ってプレイヤーを追いかける
	g_cam.k = std::min(1.0f, 860.0f / (kArenaRight - kArenaLeft));
	g_cam.d = hi * g_cam.k;
	g_cam.tx = g_cam.cx;
	g_cam.tz = g_cam.cz;
}

void FollowCamera(const Vec2& target, bool snap)
{
	const float mx = (kArenaRight - kArenaLeft) * 0.5f * (1.0f - g_cam.k) + 20.0f;
	const float mz = (kArenaBottom - kArenaTop) * 0.5f * (1.0f - g_cam.k) + 20.0f;
	const float gx = std::clamp(target.x, g_cam.cx - mx, g_cam.cx + mx);
	const float gz = std::clamp(target.y, g_cam.cz - mz, g_cam.cz + mz);
	const float t = snap ? 1.0f : 0.12f;
	g_cam.tx += (gx - g_cam.tx) * t;
	g_cam.tz += (gz - g_cam.tz) * t;
}

void Begin(const Vec2& shake)
{
	ApplyCamera(g_cam.tx + shake.x, g_cam.tz + shake.y);
	SetUseLighting(FALSE); // 陰影は自前で頂点色に焼き込む
	SetUseZBuffer3D(TRUE);
	SetWriteZBuffer3D(TRUE);
}

void End()
{
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetUseZBuffer3D(FALSE);
	SetWriteZBuffer3D(FALSE);
}

Vec2 Project(float x, float h, float y)
{
	VECTOR s = ConvWorldPosToScreenPos(ToDx(x, h, y));
	return { s.x, s.y };
}

void Part(const Pose& pose, Shape shape, VECTOR local, VECTOR scale, Rgb color, float emissive, Blend blend, int alpha, VECTOR rot)
{
	const Mesh& m = g_mesh[static_cast<int>(shape)];
	const float yaw = std::atan2(pose.forward.x, -pose.forward.y);
	const MATRIX poseRot = MGetRotY(yaw);
	const MATRIX rotation = MMult(MMult(MMult(MGetRotZ(rot.z), MGetRotX(rot.x)), MGetRotY(rot.y)), poseRot);
	scale = VGet(std::max(scale.x, 1e-3f), std::max(scale.y, 1e-3f), std::max(scale.z, 1e-3f));
	const VECTOR origin = VAdd(ToDx(pose.pos.x, 0.0f, pose.pos.y), VTransform(local, poseRot));
	const MATRIX world = MMult(MMult(MGetScale(scale), rotation), MGetTranslate(origin));

	g_buf.resize(m.pos.size());
	// 半透明は頂点アルファ、加算はブレンドの強さで薄くする
	const int a = blend == Blend::Alpha ? alpha : 255;
	for (size_t i = 0; i < m.pos.size(); ++i) {
		VERTEX3D& v = g_buf[i];
		v.pos = VTransform(m.pos[i], world);
		// 拡大率が軸ごとに違うときの法線は逆数で補正する
		const VECTOR n0 = m.norm[i];
		v.norm = VNorm(VTransformSR(VGet(n0.x / scale.x, n0.y / scale.y, n0.z / scale.z), rotation));
		v.dif = (blend == Blend::Add || blend == Blend::Mul) ? GetColorU8(color.r, color.g, color.b, a) : Lit(color, v.norm, emissive, a);
		v.spc = GetColorU8(0, 0, 0, 0);
		v.u = v.v = v.su = v.sv = 0.0f;
	}
	SetBlend(blend, (blend == Blend::Add || blend == Blend::Mul) ? alpha : 255);
	DrawPolygonIndexed3D(g_buf.data(), static_cast<int>(g_buf.size()), m.idx.data(), static_cast<int>(m.idx.size() / 3),
		DX_NONE_GRAPH, blend != Blend::Opaque);
	ResetBlend();
}

void Beam(const Vec2& from, const Vec2& to, float h, float width, float height, Rgb color, float emissive)
{
	Vec2 d = to - from;
	float len = d.Length();
	if (len < 0.5f) return;
	Part({ (from + to) * 0.5f, d / len }, Shape::Box, VGet(0.0f, h, 0.0f), VGet(width, height, len), color, emissive);
}

void Shadow(const Vec2& c, float r)
{
	// 平たい円柱を乗算で重ねて床を暗くする（3D の半透明描画では頂点色が反映されないため）
	Part({ c }, Shape::Cylinder, VGet(0.0f, 0.5f, 0.0f), VGet(r, 0.4f, r * 0.85f), { 175, 175, 185 }, 1.0f, Blend::Mul, 255);
}

void GroundDisc(const Vec2& c, float r, Rgb color, int alpha, bool add, float h)
{
	const COLOR_U8 col = GetColorU8(color.r, color.g, color.b, add ? 255 : alpha);
	const int seg = 28;
	for (int i = 0; i < seg; ++i) {
		float a0 = kPi * 2.0f * i / seg, a1 = kPi * 2.0f * (i + 1) / seg;
		g_ground.push_back(GroundVertex(c.x, c.y, h, col));
		g_ground.push_back(GroundVertex(c.x + std::cos(a0) * r, c.y + std::sin(a0) * r, h, col));
		g_ground.push_back(GroundVertex(c.x + std::cos(a1) * r, c.y + std::sin(a1) * r, h, col));
	}
	DrawGround(add ? Blend::Add : Blend::Alpha, add ? alpha : 255);
}

void GroundRing(const Vec2& c, float r, float width, Rgb color, int alpha, bool add, float h)
{
	const COLOR_U8 col = GetColorU8(color.r, color.g, color.b, add ? 255 : alpha);
	const float r0 = std::max(0.0f, r - width);
	const int seg = 48;
	for (int i = 0; i < seg; ++i) {
		float a0 = kPi * 2.0f * i / seg, a1 = kPi * 2.0f * (i + 1) / seg;
		Vec2 i0 = c + Vec2::FromAngle(a0) * r0, i1 = c + Vec2::FromAngle(a1) * r0;
		Vec2 o0 = c + Vec2::FromAngle(a0) * r, o1 = c + Vec2::FromAngle(a1) * r;
		g_ground.push_back(GroundVertex(i0.x, i0.y, h, col));
		g_ground.push_back(GroundVertex(o0.x, o0.y, h, col));
		g_ground.push_back(GroundVertex(o1.x, o1.y, h, col));
		g_ground.push_back(GroundVertex(i0.x, i0.y, h, col));
		g_ground.push_back(GroundVertex(o1.x, o1.y, h, col));
		g_ground.push_back(GroundVertex(i1.x, i1.y, h, col));
	}
	DrawGround(add ? Blend::Add : Blend::Alpha, add ? alpha : 255);
}

void GroundFan(const Vec2& c, float r, float a0, float a1, Rgb color, int alpha)
{
	const COLOR_U8 col = GetColorU8(color.r, color.g, color.b, 255);
	const int seg = std::max(2, static_cast<int>(std::fabs(a1 - a0) / 0.1f));
	for (int i = 0; i < seg; ++i) {
		float t0 = a0 + (a1 - a0) * i / seg, t1 = a0 + (a1 - a0) * (i + 1) / seg;
		g_ground.push_back(GroundVertex(c.x, c.y, 4.0f, col));
		g_ground.push_back(GroundVertex(c.x + std::cos(t0) * r, c.y + std::sin(t0) * r, 4.0f, col));
		g_ground.push_back(GroundVertex(c.x + std::cos(t1) * r, c.y + std::sin(t1) * r, 4.0f, col));
	}
	DrawGround(Blend::Add, alpha);
}

void PushQuad(std::vector<VERTEX3D>& out, float x0, float y0, float x1, float y1, float h, Rgb color)
{
	const COLOR_U8 col = Lit(color, VGet(0.0f, 1.0f, 0.0f), 0.0f, 255);
	const VERTEX3D a = GroundVertex(x0, y0, h, col), b = GroundVertex(x1, y0, h, col);
	const VERTEX3D c = GroundVertex(x1, y1, h, col), d = GroundVertex(x0, y1, h, col);
	out.insert(out.end(), { a, b, c, a, c, d });
}

void DrawTriangles(const std::vector<VERTEX3D>& verts)
{
	if (verts.empty()) return;
	DrawPolygon3D(verts.data(), static_cast<int>(verts.size() / 3), DX_NONE_GRAPH, FALSE);
}

}
