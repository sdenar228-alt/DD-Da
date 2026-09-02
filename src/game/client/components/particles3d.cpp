#include "particles3d.h"

#include <base/math.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace {

// A tiny deterministic generator, so the field looks the same on every start
// and needs no random state saved anywhere.
class CHash
{
	unsigned m_State;

public:
	explicit CHash(unsigned Seed) :
		m_State(Seed * 747796405u + 2891336453u) {}
	float Next()
	{
		m_State = m_State * 747796405u + 2891336453u;
		unsigned Word = ((m_State >> ((m_State >> 28u) + 4u)) ^ m_State) * 277803737u;
		return ((Word >> 22u) ^ Word) / 1024.0f / 1024.0f / 1024.0f / 4.0f + 0.5f;
	}
};

// The wireframes, all inside a unit box around the origin. A shape is a list of
// 3d points and a list of index pairs that make its edges.
struct SWire
{
	const vec3 *m_pPoints;
	int m_NumPoints;
	const int *m_pEdges; // pairs
	int m_NumEdges;
	// Extruded from a flat ring, so its thickness follows the setting.
	bool m_Prism;
};

const vec3 gs_aCubePoints[] = {
	{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
	{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}};
const int gs_aCubeEdges[] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};

// The flat shapes are not left flat. Seen edge on, a ring collapses into a
// line, and a field of them reads as scribbles rather than as shapes. Each one
// is extruded into a prism instead: the ring at either face, with struts
// between them. The faces sit at z = -1 and 1 here and are scaled to the wanted
// thickness at drawing time, so the slider moves them live.
constexpr int HEART_RING = 24;
constexpr int CIRCLE_RING = 20;
constexpr int HEX_RING = 6;
constexpr int TRI_RING = 3;

// A strut every few points rather than at every point: twenty four struts
// around a heart is a fence, not a heart.
constexpr int HEART_STRIDE = 3;
constexpr int CIRCLE_STRIDE = 4;
constexpr int HEX_STRIDE = 1;
constexpr int TRI_STRIDE = 1;

constexpr int PrismPoints(int Ring) { return Ring * 2; }
// Both rings, plus one strut per Stride points.
constexpr int PrismEdges(int Ring, int Stride) { return Ring * 2 + (Ring + Stride - 1) / Stride; }

vec3 gs_aHeartPoints[PrismPoints(HEART_RING)];
int gs_aHeartEdges[PrismEdges(HEART_RING, HEART_STRIDE) * 2];
vec3 gs_aCirclePoints[PrismPoints(CIRCLE_RING)];
int gs_aCircleEdges[PrismEdges(CIRCLE_RING, CIRCLE_STRIDE) * 2];
vec3 gs_aHexPoints[PrismPoints(HEX_RING)];
int gs_aHexEdges[PrismEdges(HEX_RING, HEX_STRIDE) * 2];
vec3 gs_aTriPoints[PrismPoints(TRI_RING)];
int gs_aTriEdges[PrismEdges(TRI_RING, TRI_STRIDE) * 2];
bool gs_WiresBuilt = false;

// What the widest shape needs, which is what the drawing buffers are sized to.
constexpr int MAX_SHAPE_POINTS = PrismPoints(HEART_RING);
constexpr int MAX_SHAPE_EDGES = PrismEdges(HEART_RING, HEART_STRIDE);

void BuildRing(vec3 *pRing, int Num, float Phase)
{
	for(int i = 0; i < Num; ++i)
	{
		const float A = Phase + i * 2.0f * pi / Num;
		pRing[i] = vec3(std::cos(A) * 0.5f, std::sin(A) * 0.5f, 0.0f);
	}
}

void BuildPrism(const vec3 *pRing, int Ring, int Stride, vec3 *pPoints, int *pEdges)
{
	for(int i = 0; i < Ring; ++i)
	{
		pPoints[i] = vec3(pRing[i].x, pRing[i].y, -1.0f);
		pPoints[Ring + i] = vec3(pRing[i].x, pRing[i].y, 1.0f);
	}
	int Edge = 0;
	const auto Add = [&](int A, int B) {
		pEdges[Edge * 2] = A;
		pEdges[Edge * 2 + 1] = B;
		++Edge;
	};
	for(int i = 0; i < Ring; ++i)
	{
		const int Next = (i + 1) % Ring;
		Add(i, Next);
		Add(Ring + i, Ring + Next);
		if(i % Stride == 0)
			Add(i, Ring + i);
	}
}

void BuildWires()
{
	if(gs_WiresBuilt)
		return;
	gs_WiresBuilt = true;

	vec3 aRing[MAX_SHAPE_POINTS];
	for(int i = 0; i < HEART_RING; ++i)
	{
		const float T = i * 2.0f * pi / HEART_RING;
		aRing[i] = vec3(
			16.0f * std::pow(std::sin(T), 3.0f) / 34.0f,
			-(13.0f * std::cos(T) - 5.0f * std::cos(2.0f * T) - 2.0f * std::cos(3.0f * T) - std::cos(4.0f * T)) / 34.0f,
			0.0f);
	}
	BuildPrism(aRing, HEART_RING, HEART_STRIDE, gs_aHeartPoints, gs_aHeartEdges);
	BuildRing(aRing, CIRCLE_RING, 0.0f);
	BuildPrism(aRing, CIRCLE_RING, CIRCLE_STRIDE, gs_aCirclePoints, gs_aCircleEdges);
	BuildRing(aRing, HEX_RING, pi / 6.0f);
	BuildPrism(aRing, HEX_RING, HEX_STRIDE, gs_aHexPoints, gs_aHexEdges);
	BuildRing(aRing, TRI_RING, pi / 2.0f);
	BuildPrism(aRing, TRI_RING, TRI_STRIDE, gs_aTriPoints, gs_aTriEdges);
}

const SWire gs_aWires[] = {
	{gs_aCubePoints, 8, gs_aCubeEdges, 12, false},
	{gs_aHeartPoints, PrismPoints(HEART_RING), gs_aHeartEdges, PrismEdges(HEART_RING, HEART_STRIDE), true},
	{gs_aCirclePoints, PrismPoints(CIRCLE_RING), gs_aCircleEdges, PrismEdges(CIRCLE_RING, CIRCLE_STRIDE), true},
	{gs_aHexPoints, PrismPoints(HEX_RING), gs_aHexEdges, PrismEdges(HEX_RING, HEX_STRIDE), true},
	{gs_aTriPoints, PrismPoints(TRI_RING), gs_aTriEdges, PrismEdges(TRI_RING, TRI_STRIDE), true}};
constexpr int NUM_KINDS = (int)std::size(gs_aWires);

vec3 Rotate3(vec3 Point, const vec3 &Angle)
{
	// X, then Y, then Z. Which order does not matter for tumbling decoration.
	float S = std::sin(Angle.x), C = std::cos(Angle.x);
	Point = vec3(Point.x, Point.y * C - Point.z * S, Point.y * S + Point.z * C);
	S = std::sin(Angle.y), C = std::cos(Angle.y);
	Point = vec3(Point.x * C + Point.z * S, Point.y, -Point.x * S + Point.z * C);
	S = std::sin(Angle.z), C = std::cos(Angle.z);
	return vec3(Point.x * C - Point.y * S, Point.x * S + Point.y * C, Point.z);
}

} // namespace

void CParticles3d::InitShapes()
{
	BuildWires();
	CHash Hash(0x1EE7u);
	for(int i = 0; i < MAX_PARTICLES; ++i)
	{
		CShape &Shape = m_aShapes[i];
		// Scattered over a wide box around wherever the camera is; the wrap in
		// OnRender pulls them into view soon enough.
		Shape.m_Pos = GameClient()->m_Camera.m_Center + vec2((Hash.Next() - 0.5f) * 2400.0f, (Hash.Next() - 0.5f) * 1400.0f);
		Shape.m_Drift = vec2((Hash.Next() - 0.5f) * 30.0f, -10.0f - Hash.Next() * 25.0f);
		Shape.m_Angle = vec3(Hash.Next() * 2.0f * pi, Hash.Next() * 2.0f * pi, Hash.Next() * 2.0f * pi);
		Shape.m_Spin = vec3((Hash.Next() - 0.5f), (Hash.Next() - 0.5f), (Hash.Next() - 0.5f)) * 1.2f;
		Shape.m_Scale = 0.5f + Hash.Next() * 0.9f;
		Shape.m_Kind = (int)(Hash.Next() * 64.0f) % NUM_KINDS;
		Shape.m_RandomColor = color_cast<ColorRGBA>(ColorHSLA(Hash.Next(), 0.9f, 0.6f));
	}
}

void CParticles3d::DrawShape(const CShape &Shape, ColorRGBA Color, float SizeBase, vec2 Offset, float Alpha) const
{
	int Kind = g_Config.m_Cl3dParticlesType;
	if(Kind >= NUM_KINDS) // mixed
		Kind = Shape.m_Kind;

	const SWire &Wire = gs_aWires[Kind];
	const float Size = SizeBase * Shape.m_Scale;

	// The extruded shapes carry their faces at plus and minus one, so this is
	// where they are given their actual thickness.
	const float Thickness = g_Config.m_Cl3dParticlesDepth / 100.0f;

	static_assert(MAX_SHAPE_POINTS >= 8, "the projection buffer must fit the cube as well");
	vec2 aProjected[MAX_SHAPE_POINTS];
	for(int i = 0; i < Wire.m_NumPoints; ++i)
	{
		vec3 Point = Wire.m_pPoints[i];
		if(Wire.m_Prism)
			Point.z *= Thickness;
		const vec3 Turned = Rotate3(Point, Shape.m_Angle);
		// A whiff of perspective, so a cube reads as a cube and not as a knot.
		const float Depth = 1.0f / (1.0f + Turned.z * Size * 0.0016f);
		aProjected[i] = Shape.m_Pos + Offset + vec2(Turned.x, Turned.y) * Size * Depth;
	}

	// One call for a whole wireframe rather than one per edge: a heart is fifty
	// six edges, and with the glow that is five wireframes for every shape.
	IGraphics::CLineItem aLines[MAX_SHAPE_EDGES];
	for(int i = 0; i < Wire.m_NumEdges; ++i)
		aLines[i] = IGraphics::CLineItem(aProjected[Wire.m_pEdges[i * 2]], aProjected[Wire.m_pEdges[i * 2 + 1]]);
	Graphics()->SetColor(Color.WithAlpha(Alpha));
	Graphics()->LinesDraw(aLines, Wire.m_NumEdges);
}

void CParticles3d::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(g_Config.m_Cl3dParticles == 0)
	{
		m_Inited = false;
		return;
	}
	if(!m_Inited)
	{
		m_Inited = true;
		InitShapes();
		m_LastTime = Client()->LocalTime();
	}

	const float Now = Client()->LocalTime();
	const float DeltaTime = std::clamp(Now - m_LastTime, 0.0f, 0.1f);
	m_LastTime = Now;

	const int Count = std::clamp(g_Config.m_Cl3dParticlesCount, 1, MAX_PARTICLES);
	const float SpeedScale = g_Config.m_Cl3dParticlesSpeed / 100.0f;
	const float SizeBase = (float)g_Config.m_Cl3dParticlesSize;
	const float Alpha = g_Config.m_Cl3dParticlesAlpha / 100.0f;

	const CScreenRect SavedScreenRect = Graphics()->GetScreen();
	const CCamera *pCamera = &GameClient()->m_Camera;
	Graphics()->MapScreen(Graphics()->MapScreenToWorld(
		pCamera->m_Center.x, pCamera->m_Center.y, 100.0f, 100.0f, 100.0f, 0, 0,
		Graphics()->ScreenAspect(), pCamera->m_Zoom));

	// The box the shapes live in: the view plus a margin, and a shape that
	// drifts out of one side comes back in on the other, so the field never
	// thins out and never pops.
	const CScreenRect View = Graphics()->GetScreen();
	const float Margin = 200.0f;
	const float Left = View.m_TopLeft.x - Margin, Right = View.m_BottomRight.x + Margin;
	const float Top = View.m_TopLeft.y - Margin, Bottom = View.m_BottomRight.y + Margin;
	const float SpanX = Right - Left, SpanY = Bottom - Top;

	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	for(int i = 0; i < Count; ++i)
	{
		CShape &Shape = m_aShapes[i];
		Shape.m_Pos += Shape.m_Drift * SpeedScale * DeltaTime;
		Shape.m_Angle += Shape.m_Spin * SpeedScale * DeltaTime;
		Shape.m_Pos.x = Left + std::fmod(std::fmod(Shape.m_Pos.x - Left, SpanX) + SpanX, SpanX);
		Shape.m_Pos.y = Top + std::fmod(std::fmod(Shape.m_Pos.y - Top, SpanY) + SpanY, SpanY);

		ColorRGBA Color;
		switch(g_Config.m_Cl3dParticlesColorMode)
		{
		case 1:
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_Cl3dParticlesColor));
			break;
		case 2:
			Color = color_cast<ColorRGBA>(ColorHSLA(std::fmod(Now * 0.1f + i * 0.05f, 1.0f), 0.9f, 0.6f));
			break;
		default:
			Color = Shape.m_RandomColor;
		}

		// The glow is the same wireframe drawn four more times, nudged around
		// itself with a faint alpha. Cheap, and reads as a glow from any
		// distance a decoration is looked at from.
		if(g_Config.m_Cl3dParticlesGlow)
		{
			const float GlowAlpha = Alpha * g_Config.m_Cl3dParticlesGlowAlpha / 100.0f;
			const float Nudge = (float)g_Config.m_Cl3dParticlesGlowOffset;
			DrawShape(Shape, Color, SizeBase, vec2(Nudge, Nudge), GlowAlpha);
			DrawShape(Shape, Color, SizeBase, vec2(-Nudge, Nudge), GlowAlpha);
			DrawShape(Shape, Color, SizeBase, vec2(Nudge, -Nudge), GlowAlpha);
			DrawShape(Shape, Color, SizeBase, vec2(-Nudge, -Nudge), GlowAlpha);
		}
		DrawShape(Shape, Color, SizeBase, vec2(0.0f, 0.0f), Alpha);
	}
	Graphics()->LinesEnd();

	Graphics()->MapScreen(SavedScreenRect);
}
