#include "tee_trail.h"

#include <base/color.h>
#include <base/math.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>

namespace {
// A jump longer than this between two frames is not movement, it is a teleport
// or a respawn, and a ribbon drawn across it would streak over the whole map.
constexpr float CUT_DISTANCE = 600.0f;
// How much of the width is left at the tail when the ribbon tapers.
constexpr float TAPER_END = 0.15f;
} // namespace

void CTeeTrail::Clear()
{
	for(auto &Trail : m_aTrails)
		Trail.clear();
}

void CTeeTrail::Sample(int ClientId, vec2 Pos, float Now, float MaxAge)
{
	std::deque<CPoint> &Trail = m_aTrails[ClientId];

	if(!Trail.empty() && distance(Trail.back().m_Pos, Pos) > CUT_DISTANCE)
		Trail.clear();

	// A standing tee should not pile up hundreds of identical points.
	if(Trail.empty() || distance(Trail.back().m_Pos, Pos) > 0.5f)
		Trail.push_back(CPoint{Pos, Now});
	else
		Trail.back().m_Time = Now;

	while(!Trail.empty() && Now - Trail.front().m_Time > MaxAge)
		Trail.pop_front();
}

ColorRGBA CTeeTrail::TrailColor(int ClientId, float Now) const
{
	switch(g_Config.m_ClTeeTrailMode)
	{
	case 1: // the tee's own colour
		return GameClient()->m_aClients[ClientId].m_RenderInfo.m_BloodColor;
	case 2: // rainbow, offset per player so a crowd is not one colour
		return color_cast<ColorRGBA>(ColorHSLA(std::fmod(Now * 0.25f + ClientId * 0.07f, 1.0f), 1.0f, 0.55f));
	case 3: // by speed, green when slow through red when flying
	{
		const std::deque<CPoint> &Trail = m_aTrails[ClientId];
		float Speed = 0.0f;
		if(Trail.size() >= 2)
		{
			const CPoint &A = Trail[Trail.size() - 2];
			const CPoint &B = Trail.back();
			const float Dt = std::max(B.m_Time - A.m_Time, 0.001f);
			Speed = distance(A.m_Pos, B.m_Pos) / Dt;
		}
		const float Heat = std::clamp(Speed / 1500.0f, 0.0f, 1.0f);
		return color_cast<ColorRGBA>(ColorHSLA((1.0f - Heat) * 0.33f, 1.0f, 0.5f));
	}
	default:
		return color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClTeeTrailColor));
	}
}

void CTeeTrail::Draw(int ClientId, float Now, float MaxAge) const
{
	const std::deque<CPoint> &Trail = m_aTrails[ClientId];
	if(Trail.size() < 2)
		return;

	const ColorRGBA Color = TrailColor(ClientId, Now);
	const float Alpha = g_Config.m_ClTeeTrailAlpha / 100.0f;
	const float HalfWidth = g_Config.m_ClTeeTrailWidth / 2.0f;
	const bool Fade = g_Config.m_ClTeeTrailFade != 0;
	const bool Taper = g_Config.m_ClTeeTrailTaper != 0;

	// How wide and how transparent the ribbon is at one of its points, from how
	// much of its life that point has behind it.
	const auto PointShape = [&](const CPoint &Point, float &Width, float &PointAlpha) {
		const float Age = std::clamp((Now - Point.m_Time) / MaxAge, 0.0f, 1.0f);
		Width = HalfWidth * (Taper ? mix(1.0f, TAPER_END, Age) : 1.0f);
		PointAlpha = Alpha * (Fade ? 1.0f - Age : 1.0f);
	};

	// The normal of a point is taken across its neighbours, so the joints of the
	// ribbon bend instead of cracking open on every turn.
	const auto NormalAt = [&](size_t i) {
		const vec2 &Before = Trail[i > 0 ? i - 1 : i].m_Pos;
		const vec2 &After = Trail[i + 1 < Trail.size() ? i + 1 : i].m_Pos;
		const vec2 Dir = After - Before;
		const float Length = length(Dir);
		return Length > 0.0001f ? vec2(-Dir.y / Length, Dir.x / Length) : vec2(0.0f, -1.0f);
	};

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(size_t i = 0; i + 1 < Trail.size(); ++i)
	{
		float WidthA, AlphaA, WidthB, AlphaB;
		PointShape(Trail[i], WidthA, AlphaA);
		PointShape(Trail[i + 1], WidthB, AlphaB);
		const vec2 NormalA = NormalAt(i) * WidthA;
		const vec2 NormalB = NormalAt(i + 1) * WidthB;
		const vec2 &PosA = Trail[i].m_Pos;
		const vec2 &PosB = Trail[i + 1].m_Pos;

		Graphics()->SetColor4(
			Color.WithAlpha(AlphaA), Color.WithAlpha(AlphaA),
			Color.WithAlpha(AlphaB), Color.WithAlpha(AlphaB));
		const IGraphics::CFreeformItem Item(
			PosA.x + NormalA.x, PosA.y + NormalA.y,
			PosA.x - NormalA.x, PosA.y - NormalA.y,
			PosB.x + NormalB.x, PosB.y + NormalB.y,
			PosB.x - NormalB.x, PosB.y - NormalB.y);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	}
	Graphics()->QuadsEnd();
}

void CTeeTrail::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(g_Config.m_ClTeeTrail == 0)
	{
		// Not cleared lazily on the off chance the setting flips back on: stale
		// seconds-old points would draw a streak from where the tee used to be.
		for(const auto &Trail : m_aTrails)
		{
			if(!Trail.empty())
			{
				const_cast<CTeeTrail *>(this)->Clear();
				break;
			}
		}
		return;
	}

	const float Now = Client()->LocalTime();
	const float MaxAge = g_Config.m_ClTeeTrailLength / (float)Client()->GameTickSpeed();

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		const bool Own = i == GameClient()->m_aLocalIds[0] || i == GameClient()->m_aLocalIds[1];
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active || (!Own && g_Config.m_ClTeeTrailOthers == 0))
		{
			m_aTrails[i].clear();
			continue;
		}
		Sample(i, GameClient()->m_aClients[i].m_RenderPos, Now, MaxAge);
	}

	const CScreenRect SavedScreenRect = Graphics()->GetScreen();
	const CCamera *pCamera = &GameClient()->m_Camera;
	Graphics()->MapScreen(Graphics()->MapScreenToWorld(
		pCamera->m_Center.x, pCamera->m_Center.y, 100.0f, 100.0f, 100.0f, 0, 0,
		Graphics()->ScreenAspect(), pCamera->m_Zoom));

	for(int i = 0; i < MAX_CLIENTS; ++i)
		Draw(i, Now, MaxAge);

	Graphics()->MapScreen(SavedScreenRect);
}
