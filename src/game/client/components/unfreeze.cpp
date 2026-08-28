#include "unfreeze.h"

#include <base/math.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/ui.h>
#include <game/collision.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>

void CUnfreeze::OnConsoleInit()
{
	Console()->Register("unfreeze_shoot", "", CFGFLAG_CLIENT, ConUnfreezeShoot, this,
		"Take the unfreeze shot the module has found");
}

void CUnfreeze::ConUnfreezeShoot(IConsole::IResult *pResult, void *pUserData)
{
	CUnfreeze *pSelf = static_cast<CUnfreeze *>(pUserData);
	if(pSelf->m_HasSolution)
		pSelf->m_WantShot = true;
}

void CUnfreeze::OnReset()
{
	Reset();
}

void CUnfreeze::Reset()
{
	m_HasSolution = false;
	m_WantShot = false;
	m_AimUntilTick = -1;
	m_LastShotTick = -1;
	m_ShotLandsTick = -1;
	m_vSolution.clear();
	m_vFlight.clear();
	m_FirstFlightTick = -1;
}

bool CUnfreeze::IsFreezeIndex(int Index) const
{
	if(Index < 0)
		return false;
	const CCollision *pCollision = Collision();
	const int aTiles[] = {
		pCollision->GetTileIndex(Index),
		pCollision->GetFrontTileIndex(Index),
		pCollision->GetSwitchType(Index)};
	for(const int Tile : aTiles)
	{
		if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE)
			return true;
	}
	return false;
}

bool CUnfreeze::TouchesFreeze(vec2 PrevPos, vec2 Pos) const
{
	// The same set of tiles the character itself applies: every tile swept since
	// the last tick, or the one under the tee when the sweep comes up empty.
	const std::vector<int> vIndices = Collision()->GetMapIndices(PrevPos, Pos);
	if(vIndices.empty())
		return IsFreezeIndex(Collision()->GetMapIndex(Pos));
	return std::any_of(vIndices.begin(), vIndices.end(), [this](int Index) { return IsFreezeIndex(Index); });
}

const CUnfreeze::CFlightStep *CUnfreeze::FlightAt(int Tick) const
{
	const int Index = Tick - m_FirstFlightTick;
	if(Index < 0 || (size_t)Index >= m_vFlight.size())
		return nullptr;
	return &m_vFlight[Index];
}

bool CUnfreeze::Predict(int LocalId, int StartTick)
{
	CGameClient *pGameClient = GameClient();
	CGameWorld *pSource = &pGameClient->m_PredictedWorld;

	// CopyWorld hijacks the links the client uses to draw its own predicted
	// entities smoothly, so they are put back right after the copy. Without that
	// every search would make predicted lasers and projectiles jump.
	CGameWorld *pSourceChild = pSource->m_pChild;
	const bool SourceChildValid = pSourceChild != nullptr && pSourceChild->m_IsValidCopy;
	std::vector<std::pair<CEntity *, CEntity *>> vSourceChildren;
	for(int Type = 0; Type < CGameWorld::NUM_ENTTYPES; Type++)
	{
		for(CEntity *pEntity = pSource->FindFirst(Type); pEntity != nullptr; pEntity = pEntity->TypeNext())
			vSourceChildren.emplace_back(pEntity, pEntity->m_pChild);
	}

	m_ScratchWorld.CopyWorld(pSource);
	m_ScratchWorldUsed = true;

	for(const auto &[pEntity, pChild] : vSourceChildren)
		pEntity->m_pChild = pChild;
	pSource->m_pChild = pSourceChild;
	if(pSourceChild != nullptr)
		pSourceChild->m_IsValidCopy = SourceChildValid;

	// Cutting the copy loose from the original. Both links have to go: the copy
	// constructor carries the source's child pointer over, which aims at an
	// entity of another world that may already be gone, and removing an entity
	// writes through both of them.
	m_ScratchWorld.m_pParent = nullptr;
	m_ScratchWorld.m_IsValidCopy = false;
	for(int Type = 0; Type < CGameWorld::NUM_ENTTYPES; Type++)
	{
		for(CEntity *pEntity = m_ScratchWorld.FindFirst(Type); pEntity != nullptr; pEntity = pEntity->TypeNext())
		{
			pEntity->m_pParent = nullptr;
			pEntity->m_pChild = nullptr;
		}
	}
	m_ScratchWorld.m_LocalClientId = LocalId;
	// The whole question the module asks is about freeze and tiles, so they are
	// simulated even when the player has turned their prediction off.
	m_ScratchWorld.m_WorldConfig.m_PredictFreeze = 1;
	m_ScratchWorld.m_WorldConfig.m_PredictTiles = true;

	CCharacter *pChar = m_ScratchWorld.GetCharacterById(LocalId);
	if(pChar == nullptr)
		return false;
	// The client lets the local tee move on the last predicted ticks of a freeze
	// to hide latency. That would make the flight wrong.
	pChar->m_CanMoveInFreeze = false;
	pChar->ResetInput();

	CNetObj_PlayerInput Input = GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy];
	// Chatting makes the character drop the input entirely, and an odd fire
	// counter would make it shoot inside the simulation.
	Input.m_PlayerFlags = 0;
	Input.m_Fire &= ~1;
	if(Input.m_TargetX == 0 && Input.m_TargetY == 0)
		Input.m_TargetY = -1;

	const int Horizon = g_Config.m_ClUnfreezeHorizon;
	m_vFlight.clear();
	m_vFlight.reserve(Horizon);
	m_FirstFlightTick = StartTick + 1;

	bool AnyUseful = false;
	m_LastUsefulTick = -1;
	for(int Tick = StartTick + 1; Tick <= StartTick + Horizon; Tick++)
	{
		m_ScratchWorld.m_GameTick = Tick;
		pChar->OnPredictedInput(&Input);
		m_ScratchWorld.Tick();

		if(m_ScratchWorld.GetCharacterById(LocalId) != pChar)
			break;

		CFlightStep Step;
		Step.m_Pos = pChar->Core()->m_Pos;
		Step.m_PrevPos = pChar->m_PrevPos;
		Step.m_Frozen = pChar->m_FreezeTime > 0;
		Step.m_Deep = pChar->Core()->m_DeepFrozen;
		Step.m_OnFreeze = TouchesFreeze(Step.m_PrevPos, Step.m_Pos);
		if(Step.m_Frozen && !Step.m_Deep && !Step.m_OnFreeze)
		{
			AnyUseful = true;
			m_LastUsefulTick = Tick;
		}
		m_vFlight.push_back(Step);
	}

	return AnyUseful;
}

void CUnfreeze::TraceLaser(vec2 Pos, vec2 Dir, float Energy, int FireTick, int MaxBounces, float BounceCost, int BounceTicks)
{
	m_vSegments.clear();
	int Bounces = 0;
	for(int Segment = 0; Segment <= MaxBounces; Segment++)
	{
		if(Energy < 0.0f)
			break;

		const vec2 Target = Pos + Dir * Energy;
		vec2 Collision2 = Target;
		vec2 To = Target;
		const int Res = Collision()->IntersectLineTeleWeapon(Pos, Target, &Collision2, &To);

		CSegment Item;
		Item.m_From = Pos;
		Item.m_To = To;
		Item.m_Tick = FireTick + Segment * BounceTicks;
		Item.m_Bounces = Bounces;
		m_vSegments.push_back(Item);

		if(!Res)
			break;

		// The reflection is the same trick the laser uses: push a short vector
		// into the wall and let the map collision flip whichever axis it hits.
		vec2 TempPos = To;
		vec2 TempDir = Dir * 4.0f;
		Collision()->MovePoint(&TempPos, &TempDir, 1.0f, nullptr);
		if(length(TempDir) < 0.001f)
			break;

		Energy -= distance(Pos, TempPos) + BounceCost;
		Pos = TempPos;
		Dir = normalize(TempDir);
		Bounces++;
	}
}

// A shot is only followed as far as it could still do something: every bounce
// costs eight ticks, so following it past the last tick worth hitting is work
// thrown away.
// A shot takes its bounce behaviour from the tune zone it is fired in, sampled
// once at the muzzle the way the laser does it, so a map that slows lasers down
// in one room is followed correctly.
const CTuningParams *CUnfreeze::LaserTuning(vec2 FirePos) const
{
	CGameWorld *pWorld = &GameClient()->m_PredictedWorld;
	if(!pWorld->m_WorldConfig.m_UseTuneZones)
		return &GameClient()->m_aTuning[g_Config.m_ClDummy];
	return pWorld->GetTuning(Collision()->IsTune(Collision()->GetMapIndex(FirePos)));
}

// How far the shot reaches comes from the tee's zone instead, not the shot's.
const CTuningParams *CUnfreeze::ReachTuning(vec2 FirePos) const
{
	CGameWorld *pWorld = &GameClient()->m_PredictedWorld;
	const CCharacter *pChar = pWorld->GetCharacterById(GameClient()->m_Snap.m_LocalClientId);
	if(!pWorld->m_WorldConfig.m_UseTuneZones || pChar == nullptr)
		return LaserTuning(FirePos);
	return pWorld->GetTuning(pChar->GetOverriddenTuneZone());
}

int CUnfreeze::BounceBudget(int FireTick, int BounceTicks, int TunedBounces) const
{
	const int Wanted = std::clamp(g_Config.m_ClUnfreezeBounces, 1, std::max(1, TunedBounces));
	if(m_LastUsefulTick < 0)
		return Wanted;
	const int Reachable = (m_LastUsefulTick + 1 - FireTick) / std::max(1, BounceTicks) + 1;
	return std::clamp(Reachable, 1, Wanted);
}

bool CUnfreeze::IsUsefulTick(int Tick) const
{
	const CFlightStep *pStep = FlightAt(Tick);
	if(pStep == nullptr || pStep->m_Deep || pStep->m_OnFreeze)
		return false;
	return pStep->m_Frozen;
}

// Traces the one angle that was found and reports whether it still lands, which
// is all that has to be redone when only the tick has moved on.
bool CUnfreeze::Revalidate(int FireTick, vec2 FirePos)
{
	if(!m_HasSolution)
		return false;

	const CTuningParams *pTuning = LaserTuning(FirePos);
	const int TunedBounces = pTuning->m_LaserBounceNum;
	const int BounceTicks = std::max(1, (int)(Client()->GameTickSpeed() * (int)pTuning->m_LaserBounceDelay / 1000) + 1);
	const int MaxBounces = BounceBudget(FireTick, BounceTicks, TunedBounces);
	TraceLaser(FirePos, m_SolutionDir, ReachTuning(FirePos)->m_LaserReach, FireTick, MaxBounces, pTuning->m_LaserBounceCost, BounceTicks);

	for(const CSegment &Segment : m_vSegments)
	{
		if(Segment.m_Bounces < 1)
			continue;
		bool HitsAimedTick = false;
		int Margin = 0;
		for(int Offset = -1; Offset <= 1; Offset++)
		{
			if(!IsUsefulTick(Segment.m_Tick - 1 + Offset))
				continue;
			const CFlightStep *pStep = FlightAt(Segment.m_Tick - 1 + Offset);
			vec2 Closest;
			closest_point_on_line(Segment.m_From, Segment.m_To, pStep->m_Pos, Closest);
			if(distance(pStep->m_Pos, Closest) >= CCharacterCore::PhysicalSize())
				continue;
			Margin++;
			HitsAimedTick = HitsAimedTick || Offset == 0;
		}
		if(!HitsAimedTick)
			continue;
		m_SolutionHitTick = Segment.m_Tick;
		m_SolutionMargin = Margin;
		m_SolutionHitPos = FlightAt(Segment.m_Tick - 1)->m_Pos;
		m_vSolution = m_vSegments;
		return true;
	}

	m_HasSolution = false;
	m_vSolution.clear();
	return false;
}

bool CUnfreeze::Search(int FireTick, vec2 FirePos)
{
	const CTuningParams *pTuning = LaserTuning(FirePos);
	const float Energy = ReachTuning(FirePos)->m_LaserReach;
	const float BounceCost = pTuning->m_LaserBounceCost;
	const int TunedBounces = pTuning->m_LaserBounceNum;
	// A bounce happens once the delay has been exceeded, not once it is reached,
	// which turns the default 150 ms into 8 ticks rather than 7.
	const int BounceTicks = std::max(1, (int)(Client()->GameTickSpeed() * (int)pTuning->m_LaserBounceDelay / 1000) + 1);
	const int MaxBounces = BounceBudget(FireTick, BounceTicks, TunedBounces);
	const int Steps = std::clamp(g_Config.m_ClUnfreezeSteps, 60, 3600);

	bool Found = false;
	int BestTick = 0;
	int BestMargin = -1;
	vec2 BestDir = vec2(1.0f, 0.0f);
	vec2 BestHit = vec2(0.0f, 0.0f);

	for(int Step = 0; Step < Steps; Step++)
	{
		const float Angle = (float)Step * 2.0f * pi / (float)Steps;
		TraceLaser(FirePos, direction(Angle), Energy, FireTick, MaxBounces, BounceCost, BounceTicks);

		for(const CSegment &Segment : m_vSegments)
		{
			// A shot cannot touch the tee that fired it before its first wall.
			if(Segment.m_Bounces < 1)
				continue;

			// The hit test runs against the position the tee had one tick before,
			// because a character only publishes its position after the lasers
			// have already moved.
			bool HitsAimedTick = false;
			int Margin = 0;
			for(int Offset = -1; Offset <= 1; Offset++)
			{
				if(!IsUsefulTick(Segment.m_Tick - 1 + Offset))
					continue;
				const CFlightStep *pStep = FlightAt(Segment.m_Tick - 1 + Offset);

				vec2 Closest;
				closest_point_on_line(Segment.m_From, Segment.m_To, pStep->m_Pos, Closest);
				if(distance(pStep->m_Pos, Closest) >= CCharacterCore::PhysicalSize())
					continue;

				Margin++;
				HitsAimedTick = HitsAimedTick || Offset == 0;
			}

			// The neighbouring ticks only say how much room for error the shot
			// has; a shot that misses the tick it is aimed at is not a plan.
			if(!HitsAimedTick)
				continue;

			// The earliest unfreeze wins, and among those the shot that survives
			// the flight being a tick early or late.
			const bool Better = !Found || Segment.m_Tick < BestTick ||
					    (Segment.m_Tick == BestTick && Margin > BestMargin);
			if(!Better)
				continue;

			Found = true;
			BestTick = Segment.m_Tick;
			BestMargin = Margin;
			// What is aimed at is where the shot leaves, not where it bounces.
			BestDir = direction(Angle);
			BestHit = FlightAt(Segment.m_Tick - 1)->m_Pos;
			// Keep the path of this candidate for drawing.
			m_vSolution = m_vSegments;
			break;
		}

		if(Found && BestMargin >= 3)
			break;
	}

	if(!Found)
		return false;

	m_HasSolution = true;
	m_SolutionDir = BestDir;
	m_SolutionHitTick = BestTick;
	m_SolutionHitPos = BestHit;
	m_SolutionMargin = BestMargin;
	return true;
}

void CUnfreeze::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		Reset();
		return;
	}
	if(g_Config.m_ClUnfreeze == 0)
	{
		Reset();
		return;
	}
	m_Status = EStatus::QUIET;

	CGameClient *pGameClient = GameClient();
	const int LocalId = pGameClient->m_Snap.m_LocalClientId;
	const CNetObj_Character *pLocal = pGameClient->m_Snap.m_pLocalCharacter;
	if(LocalId < 0 || pLocal == nullptr || pGameClient->m_Snap.m_SpecInfo.m_Active)
	{
		Reset();
		return;
	}

	const int PredTick = Client()->PredGameTick(g_Config.m_ClDummy);
	const CCharacter *pPredicted = pGameClient->m_PredictedWorld.GetCharacterById(LocalId);
	// While already frozen there is nothing to look for: the shot cannot be taken
	// any more, it had to leave before the freeze started.
	const bool CanShoot = pPredicted != nullptr && pPredicted->m_FreezeTime <= 0;

	if(!CanShoot)
	{
		m_HasSolution = false;
		m_vSolution.clear();
		// Frozen already: the shot had to leave before this.
		m_Status = pPredicted != nullptr ? EStatus::TOO_LATE : EStatus::QUIET;
		RenderStatus();
		return;
	}

	const float Now = Client()->LocalTime();
	const float Interval = std::clamp(g_Config.m_ClUnfreezeInterval, 20, 500) / 1000.0f;
	const bool Searched = Now - m_LastSearchTime >= Interval || Now < m_LastSearchTime;
	if(Searched)
	{
		m_LastSearchTime = Now;
		m_HasSolution = false;
		m_vSolution.clear();
		// The shot leaves with the input that is being built now, so it is cast
		// from the tick after the one the prediction ends on.
		m_FreezeAhead = Predict(LocalId, PredTick);
		if(m_FreezeAhead)
			Search(PredTick + 1, pPredicted->Core()->m_Pos);
	}

	// A plan is made for one tick, so once it is older than that it is checked
	// again against the tick it would actually leave on now.
	if(m_HasSolution && !Searched)
		Revalidate(PredTick + 1, pPredicted->Core()->m_Pos);

	// Automatic mode fires as soon as it has a plan, then holds off until the
	// shot already on its way has arrived and the weapon has reloaded.
	if(g_Config.m_ClUnfreeze == 2 && m_HasSolution && !m_WantShot)
	{
		const int FireDelay = (int)(GameClient()->m_aTuning[g_Config.m_ClDummy].GetWeaponFireDelay(WEAPON_LASER) * Client()->GameTickSpeed());
		const bool Reloaded = Client()->GameTick(g_Config.m_ClDummy) - pLocal->m_AttackTick >= FireDelay;
		const bool Cooled = m_LastShotTick < 0 || PredTick - m_LastShotTick >= FireDelay;
		const bool Landed = PredTick >= m_ShotLandsTick;
		if(Reloaded && Cooled && Landed)
			m_WantShot = true;
	}

	if(m_HasSolution)
	{
		const bool HoldsLaser = pLocal->m_Weapon == WEAPON_LASER;
		m_Status = HoldsLaser || g_Config.m_ClUnfreezeSwitchWeapon ? EStatus::READY : EStatus::NO_LASER;
	}
	else if(m_FreezeAhead)
	{
		m_Status = EStatus::SEARCHING;
	}

	RenderPlan();
	RenderStatus();
}

void CUnfreeze::RenderStatus() const
{
	if(!g_Config.m_ClUnfreezeShowStatus || m_Status == EStatus::QUIET)
		return;

	const char *pText = nullptr;
	ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
	switch(m_Status)
	{
	case EStatus::READY:
		pText = g_Config.m_ClUnfreeze == 2 ? Localize("Unfreeze shot ready") : Localize("Unfreeze shot ready, press your unfreeze key");
		Color = ColorRGBA(0.35f, 0.9f, 0.45f, 1.0f);
		break;
	case EStatus::SEARCHING:
		pText = Localize("Freeze ahead, no shot found");
		Color = ColorRGBA(1.0f, 0.75f, 0.3f, 1.0f);
		break;
	case EStatus::NO_LASER:
		pText = Localize("Unfreeze shot found, but you are not holding the laser");
		Color = ColorRGBA(1.0f, 0.6f, 0.3f, 1.0f);
		break;
	case EStatus::TOO_LATE:
		pText = Localize("Frozen already, the shot had to leave earlier");
		Color = ColorRGBA(0.7f, 0.75f, 0.85f, 1.0f);
		break;
	default:
		return;
	}

	const CScreenRect SavedScreenRect = Graphics()->GetScreen();
	Ui()->MapScreen();
	const float FontSize = 9.0f;
	const CUIRect *pScreen = Ui()->Screen();
	CUIRect Line = {0.0f, pScreen->h * 0.62f, pScreen->w, FontSize};
	TextRender()->TextColor(Color);
	Ui()->DoLabel(&Line, pText, FontSize, TEXTALIGN_MC);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(SavedScreenRect);
}

bool CUnfreeze::ApplyInput(CNetObj_PlayerInput *pInput)
{
	if(!m_HasSolution)
		return false;

	const bool HoldsLaser = GameClient()->m_Snap.m_pLocalCharacter != nullptr &&
				GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_LASER;
	if(!HoldsLaser)
	{
		if(!g_Config.m_ClUnfreezeSwitchWeapon)
			return false;
		// Asking for the laser is all that can be done this tick, the shot waits
		// for the snapshot that confirms the switch.
		if(pInput->m_WantedWeapon == WEAPON_LASER + 1)
			return false;
		pInput->m_WantedWeapon = WEAPON_LASER + 1;
		return true;
	}

	if(!m_WantShot)
		return false;
	m_WantShot = false;

	// An even step leaves the parity alone, so the player's own fire key stays in
	// step with the counter, and the laser's full automatic mode is not armed.
	pInput->m_Fire = (pInput->m_Fire + 2) & INPUT_STATE_MASK;
	m_LastShotTick = Client()->PredGameTick(g_Config.m_ClDummy);
	m_AimUntilTick = m_LastShotTick + 3;
	m_ShotLandsTick = m_SolutionHitTick + 1;
	return true;
}

bool CUnfreeze::ApplyAim(CNetObj_PlayerInput *pInput) const
{
	if(!m_HasSolution || Client()->PredGameTick(g_Config.m_ClDummy) > m_AimUntilTick)
		return false;

	// Only the direction matters to the shot, but the target is sent as whole
	// units, so a short vector would round the angle into something else: at a
	// length of 1 every angle becomes a multiple of 45 degrees. The length is
	// therefore stretched to at least 500, which the server accepts and which
	// nothing else here reads.
	const float Radius = std::clamp(length(vec2(pInput->m_TargetX, pInput->m_TargetY)), 500.0f, 2000.0f);
	pInput->m_TargetX = round_to_int(m_SolutionDir.x * Radius);
	pInput->m_TargetY = round_to_int(m_SolutionDir.y * Radius);
	if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
		pInput->m_TargetY = -1;
	return true;
}

void CUnfreeze::RenderPlan() const
{
	if(!m_HasSolution || (!g_Config.m_ClUnfreezeShowPath && !g_Config.m_ClUnfreezeShowFlight))
		return;

	const CScreenRect SavedScreenRect = Graphics()->GetScreen();
	const CCamera *pCamera = &GameClient()->m_Camera;
	Graphics()->MapScreen(Graphics()->MapScreenToWorld(
		pCamera->m_Center.x, pCamera->m_Center.y, 100.0f, 100.0f, 100.0f, 0, 0,
		Graphics()->ScreenAspect(), pCamera->m_Zoom));

	const ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClUnfreezeColor, true));
	Graphics()->TextureClear();

	if(g_Config.m_ClUnfreezeShowFlight)
	{
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Color.WithMultipliedAlpha(0.35f));
		for(const CFlightStep &Step : m_vFlight)
		{
			if(!Step.m_Frozen)
				continue;
			const IGraphics::CQuadItem QuadItem(Step.m_Pos.x - 2.0f, Step.m_Pos.y - 2.0f, 4.0f, 4.0f);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
		}
		Graphics()->QuadsEnd();
	}

	if(g_Config.m_ClUnfreezeShowPath)
	{
		Graphics()->LinesBegin();
		for(const CSegment &Segment : m_vSolution)
		{
			// The stretch before the first wall can never come back to the tee,
			// so it is drawn faded to keep the useful part readable.
			Graphics()->SetColor(Segment.m_Bounces < 1 ? Color.WithMultipliedAlpha(0.4f) : Color);
			const IGraphics::CLineItem LineItem(Segment.m_From.x, Segment.m_From.y, Segment.m_To.x, Segment.m_To.y);
			Graphics()->LinesDraw(&LineItem, 1);
		}
		Graphics()->LinesEnd();

		Graphics()->QuadsBegin();
		Graphics()->SetColor(Color);
		const IGraphics::CQuadItem QuadItem(m_SolutionHitPos.x - 8.0f, m_SolutionHitPos.y - 8.0f, 16.0f, 16.0f);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}

	Graphics()->MapScreen(SavedScreenRect);
}
