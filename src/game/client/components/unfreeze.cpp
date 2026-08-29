#include "unfreeze.h"

#include <base/math.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/ui.h>
#include <game/collision.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>

namespace {
// The aim is sent as whole units, so the angle that leaves the client is never
// exactly the angle that was wanted. Everything is therefore traced through the
// integer target, at a length long enough that the rounding is worth well under
// a tenth of a degree.
constexpr float AIM_RADIUS = 2000.0f;
// A hit that takes less than this off the freeze is not worth a shot and a
// reload; the tee would have thawed on its own about as soon.
constexpr int MIN_SAVED_TICKS = 8;
// Entities further away than this are dropped from the simulation. They cannot
// reach the tee inside the horizon, and ticking them is what the search costs.
constexpr float SIMULATE_RADIUS = 1600.0f;
} // namespace

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
	m_FiredTargetX = 0;
	m_FiredTargetY = 0;
	m_RestoreWeapon = -1;
	m_SwitchSends = 0;
	m_SwitchDeadlineTick = -1;
	m_vSolution.clear();
	m_vFlight.clear();
	m_FirstFlightTick = -1;
	m_FirstUsefulTick = -1;
	m_LastUsefulTick = -1;
	m_FreezeAhead = false;
	m_Status = EStatus::QUIET;
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
		// Live freeze is deliberately not here: sweeping over one does not put
		// the freeze timer back, it only sets a flag a laser cannot clear.
		if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE)
			return true;
	}
	return false;
}

bool CUnfreeze::TouchesFreeze(vec2 PrevPos, vec2 Pos) const
{
	// The same set of tiles the character itself applies, walked without the
	// vector that Collision()->GetMapIndices() would allocate on every tick of
	// every search.
	const float Length = distance(PrevPos, Pos);
	const int Samples = std::clamp((int)(Length / 16.0f) + 1, 1, 32);
	int LastIndex = -2;
	for(int i = 0; i <= Samples; ++i)
	{
		const vec2 Sample = mix(PrevPos, Pos, (float)i / (float)Samples);
		const int Index = Collision()->GetMapIndex(Sample);
		if(Index == LastIndex)
			continue;
		LastIndex = Index;
		if(IsFreezeIndex(Index))
			return true;
	}
	return false;
}

const CUnfreeze::CFlightStep *CUnfreeze::FlightAt(int Tick) const
{
	const int Index = Tick - m_FirstFlightTick;
	if(Index < 0 || (size_t)Index >= m_vFlight.size())
		return nullptr;
	return &m_vFlight[Index];
}

bool CUnfreeze::IsUsefulTick(int Tick) const
{
	const CFlightStep *pStep = FlightAt(Tick);
	if(pStep == nullptr || pStep->m_Deep || pStep->m_Live || pStep->m_FreezeTime <= 0)
		return false;
	if(pStep->m_OnFreeze)
		return false;
	// The tick after matters as well: the game re-applies the tiles in the same
	// tick as the unfreeze, so a hit one tick before the tee lands back in freeze
	// buys a fresh three seconds instead of freedom.
	const CFlightStep *pNext = FlightAt(Tick + 1);
	return pNext != nullptr && !pNext->m_OnFreeze;
}

int CUnfreeze::FreezeSaved(int Tick) const
{
	const CFlightStep *pStep = FlightAt(Tick);
	if(pStep == nullptr || pStep->m_FreezeTime <= 0)
		return 0;

	// An unfreeze does not change where the tee goes, only whether it is frozen
	// on the way, so the recorded flight is still the right timeline. The shot is
	// worth the ticks between the hit and whichever comes first: the freeze
	// running out on its own, or a tile freezing the tee again.
	const int End = m_FirstFlightTick + (int)m_vFlight.size();
	for(int i = Tick + 1; i < End; ++i)
	{
		const CFlightStep *pAt = FlightAt(i);
		if(pAt == nullptr || pAt->m_FreezeTime <= 0 || pAt->m_OnFreeze)
			return i - Tick;
	}
	return End - Tick;
}

bool CUnfreeze::SelfHitPossible() const
{
	const CGameWorld *pWorld = &GameClient()->m_PredictedWorld;
	// A laser only becomes able to touch its owner after a bounce, and only on a
	// DDRace server running the current laser.
	return !pWorld->m_WorldConfig.m_OldLaser && pWorld->m_WorldConfig.m_IsDDRace;
}

const CTuningParams *CUnfreeze::LaserTuning(vec2 FirePos) const
{
	CGameWorld *pWorld = &GameClient()->m_PredictedWorld;
	if(!pWorld->m_WorldConfig.m_UseTuneZones)
		return &GameClient()->m_aTuning[g_Config.m_ClDummy];
	return pWorld->GetTuning(Collision()->IsTune(Collision()->GetMapIndex(FirePos)));
}

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
	const int Wanted = std::clamp(g_Config.m_ClUnfreezeBounces, 0, std::max(0, TunedBounces));
	if(m_LastUsefulTick < 0)
		return Wanted;
	// Following the shot past the last tick worth hitting is work thrown away.
	const int Reachable = (m_LastUsefulTick + 2 - FireTick) / std::max(1, BounceTicks) + 2;
	return std::clamp(Reachable, 0, Wanted);
}

bool CUnfreeze::Predict(int LocalId, int StartTick, int Horizon)
{
	CGameClient *pGameClient = GameClient();
	CGameWorld *pSource = &pGameClient->m_PredictedWorld;

	// CopyWorld hijacks the links the client uses to draw its own predicted
	// entities smoothly, so they are put back right after the copy. Without that
	// every search would make predicted lasers and projectiles jump.
	CGameWorld *pSourceChild = pSource->m_pChild;
	const bool SourceChildValid = pSourceChild != nullptr && pSourceChild->m_IsValidCopy;
	static std::vector<std::pair<CEntity *, CEntity *>> s_vSourceChildren;
	s_vSourceChildren.clear();
	for(int Type = 0; Type < CGameWorld::NUM_ENTTYPES; Type++)
	{
		for(CEntity *pEntity = pSource->FindFirst(Type); pEntity != nullptr; pEntity = pEntity->TypeNext())
			s_vSourceChildren.emplace_back(pEntity, pEntity->m_pChild);
	}

	m_ScratchWorld.CopyWorld(pSource);

	for(const auto &[pEntity, pChild] : s_vSourceChildren)
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

	// Ticking a whole server's worth of entities a hundred times over is what
	// this used to cost. Nothing that far away can reach the tee inside the
	// horizon, so it is dropped before the loop rather than simulated in it.
	const vec2 Origin = pChar->Core()->m_Pos;
	static std::vector<CEntity *> s_vDrop;
	s_vDrop.clear();
	for(int Type = 0; Type < CGameWorld::NUM_ENTTYPES; Type++)
	{
		for(CEntity *pEntity = m_ScratchWorld.FindFirst(Type); pEntity != nullptr; pEntity = pEntity->TypeNext())
		{
			if(pEntity != pChar && distance(pEntity->m_Pos, Origin) > SIMULATE_RADIUS)
				s_vDrop.push_back(pEntity);
		}
	}
	for(CEntity *pEntity : s_vDrop)
		pEntity->Destroy();

	// The client lets the local tee move on the last predicted ticks of a freeze
	// to hide latency. That would make the flight wrong.
	pChar->m_CanMoveInFreeze = false;
	pChar->ResetInput();

	// Every other tee arrives holding whatever input the snapshot left on it,
	// including a fire bit that would have it shooting for the whole horizon.
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pOther = m_ScratchWorld.GetCharacterById(i);
		if(pOther != nullptr && pOther != pChar)
			pOther->ResetInput();
	}

	CNetObj_PlayerInput Input = GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy];
	// Chatting makes the character drop the input entirely, and an odd fire
	// counter would make it shoot inside the simulation.
	Input.m_PlayerFlags = 0;
	Input.m_Fire &= ~1;
	if(Input.m_TargetX == 0 && Input.m_TargetY == 0)
		Input.m_TargetY = -1;

	m_vFlight.clear();
	m_vFlight.reserve(Horizon);
	m_FirstFlightTick = StartTick + 1;
	m_FirstUsefulTick = -1;
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
		Step.m_FreezeTime = pChar->m_FreezeTime;
		Step.m_Deep = pChar->Core()->m_DeepFrozen;
		Step.m_Live = pChar->Core()->m_LiveFrozen;
		Step.m_OnFreeze = TouchesFreeze(Step.m_PrevPos, Step.m_Pos);

		for(int i = 0; i < MAX_CLIENTS && Step.m_NumOthers < MAX_TRACKED; i++)
		{
			const CCharacter *pOther = m_ScratchWorld.GetCharacterById(i);
			if(pOther == nullptr || pOther == pChar)
				continue;
			if(distance(pOther->m_Pos, Step.m_Pos) > SIMULATE_RADIUS)
				continue;
			Step.m_aOthers[Step.m_NumOthers++] = pOther->m_Pos;
		}

		m_vFlight.push_back(Step);
	}

	for(int i = 0; i < (int)m_vFlight.size(); i++)
	{
		const int Tick = m_FirstFlightTick + i;
		if(!IsUsefulTick(Tick))
			continue;
		if(m_FirstUsefulTick < 0)
			m_FirstUsefulTick = Tick;
		m_LastUsefulTick = Tick;
	}

	return m_FirstUsefulTick >= 0;
}

void CUnfreeze::TraceLaser(vec2 Pos, vec2 Dir, float Energy, int FireTick, int MaxBounces, float BounceCost, int BounceTicks)
{
	m_vSegments.clear();
	int Bounces = 0;
	bool ZeroBounceLastTick = false;
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
		// The laser's own clock starts on the tick the input is executed, which is
		// one before the tick that input is stamped with, and every bounce is
		// counted from there.
		Item.m_EvalTick = FireTick - 1 + Segment * BounceTicks;
		Item.m_Bounces = Bounces;
		m_vSegments.push_back(Item);

		if(!Res)
			break;

		// The reflection is the same trick the laser uses: push a short vector
		// into the wall and let the map collision flip whichever axis it hits.
		vec2 TempPos = To;
		vec2 TempDir = Dir * 4.0f;
		Collision()->MovePoint(&TempPos, &TempDir, 1.0f, nullptr);

		// Two bounces in a row that cover no ground kill the laser outright,
		// which is how the game stops a beam trapped in a corner.
		const float Distance = distance(Pos, TempPos);
		if(Distance == 0.0f && ZeroBounceLastTick)
			break;
		ZeroBounceLastTick = Distance == 0.0f;

		Energy -= Distance + BounceCost;
		Pos = TempPos;
		Dir = normalize(TempDir);
		Bounces++;
	}
}

CUnfreeze::CCandidate CUnfreeze::Evaluate(float Angle, int FireTick, vec2 FirePos, int MaxBounces, int BounceTicks, float Energy, float BounceCost)
{
	CCandidate Candidate;
	// Trace the direction that will actually be sent, not the one that was
	// wanted: the target is whole units on the wire.
	Candidate.m_TargetX = round_to_int(std::cos(Angle) * AIM_RADIUS);
	Candidate.m_TargetY = round_to_int(std::sin(Angle) * AIM_RADIUS);
	if(Candidate.m_TargetX == 0 && Candidate.m_TargetY == 0)
		return Candidate;
	const vec2 Dir = normalize(vec2((float)Candidate.m_TargetX, (float)Candidate.m_TargetY));

	TraceLaser(FirePos, Dir, Energy, FireTick, MaxBounces, BounceCost, BounceTicks);

	const float Radius = CCharacterCore::PhysicalSize();
	for(const CSegment &Segment : m_vSegments)
	{
		// A stretch of no length is not something the game would test against.
		if(Segment.m_From == Segment.m_To)
			continue;

		// The game matches a laser against the position a tee published at the
		// end of the previous tick.
		const CFlightStep *pStep = FlightAt(Segment.m_EvalTick - 1);
		if(pStep == nullptr)
		{
			// The first stretch is cast before the flight table starts, so there
			// is nothing to test it against yet; past the end there never will be.
			if(Segment.m_EvalTick - 1 >= m_FirstFlightTick + (int)m_vFlight.size())
				break;
			continue;
		}

		// Whoever the beam reaches first ends it, so the other tees are asked
		// before the owner can count as a hit.
		for(int i = 0; i < pStep->m_NumOthers; i++)
		{
			vec2 Closest;
			if(!closest_point_on_line(Segment.m_From, Segment.m_To, pStep->m_aOthers[i], Closest))
				continue;
			if(distance(pStep->m_aOthers[i], Closest) < Radius)
				return Candidate;
		}

		// A shot cannot touch the tee that fired it before its first wall.
		if(Segment.m_Bounces < 1)
			continue;

		vec2 Closest;
		if(!closest_point_on_line(Segment.m_From, Segment.m_To, pStep->m_Pos, Closest))
			continue;
		const float Miss = distance(pStep->m_Pos, Closest);
		if(Miss >= Radius)
			continue;

		// This is where the shot ends, whatever it is worth. If the tick is no
		// good the angle is dead, because everything after it never happens.
		if(!IsUsefulTick(Segment.m_EvalTick - 1))
			return Candidate;

		int Margin = 0;
		for(int Offset = -1; Offset <= 1; Offset++)
		{
			if(!IsUsefulTick(Segment.m_EvalTick - 1 + Offset))
				continue;
			const CFlightStep *pAt = FlightAt(Segment.m_EvalTick - 1 + Offset);
			vec2 NearBy;
			if(!closest_point_on_line(Segment.m_From, Segment.m_To, pAt->m_Pos, NearBy))
				continue;
			if(distance(pAt->m_Pos, NearBy) < Radius)
				Margin++;
		}

		Candidate.m_Hits = true;
		Candidate.m_EvalTick = Segment.m_EvalTick;
		Candidate.m_Saved = FreezeSaved(Segment.m_EvalTick - 1);
		Candidate.m_Margin = Margin;
		Candidate.m_Miss = Miss;
		Candidate.m_HitPos = pStep->m_Pos;
		return Candidate;
	}

	return Candidate;
}

// The ticks of freeze taken off are the whole point; the rest is how likely the
// shot is to survive the flight being a tick, or a fraction of a degree, away
// from what was predicted.
float CUnfreeze::ScoreOf(const CCandidate &Candidate, int FireTick, float Radius)
{
	if(!Candidate.m_Hits)
		return 0.0f;
	const float Centred = (Radius - Candidate.m_Miss) / Radius;
	return (float)Candidate.m_Saved +
	       8.0f * (float)(Candidate.m_Margin - 1) +
	       10.0f * Centred -
	       0.1f * (float)(Candidate.m_EvalTick - FireTick);
}

bool CUnfreeze::Search(int FireTick, vec2 FirePos)
{
	if(!SelfHitPossible())
		return false;

	const CTuningParams *pTuning = LaserTuning(FirePos);
	const float Energy = ReachTuning(FirePos)->m_LaserReach;
	const float BounceCost = pTuning->m_LaserBounceCost;
	const int TunedBounces = pTuning->m_LaserBounceNum;
	if(TunedBounces < 1)
		return false;

	// A bounce happens once the delay has been exceeded, not once it is reached,
	// which turns the default 150 ms into 8 ticks rather than 7.
	const int BounceTicks = std::max(1, (int)(Client()->GameTickSpeed() * (int)pTuning->m_LaserBounceDelay / 1000) + 1);
	const int MaxBounces = BounceBudget(FireTick, BounceTicks, TunedBounces);
	if(MaxBounces < 1)
		return false;

	const int Steps = std::clamp(g_Config.m_ClUnfreezeSteps, 120, 1440);
	const float Radius = CCharacterCore::PhysicalSize();

	float BestScore = 0.0f;
	float BestAngle = 0.0f;
	CCandidate Best;

	// A coarse sweep first. Every angle is judged on what it is worth rather than
	// on how soon it lands, and none is skipped: stopping at the first passable
	// shot is how this used to settle for the worst one.
	for(int Step = 0; Step < Steps; Step++)
	{
		const float Angle = (float)Step * 2.0f * pi / (float)Steps;
		const CCandidate Candidate = Evaluate(Angle, FireTick, FirePos, MaxBounces, BounceTicks, Energy, BounceCost);
		if(!Candidate.m_Hits || Candidate.m_Margin < 2 || Candidate.m_Saved < MIN_SAVED_TICKS)
			continue;
		const float Score = ScoreOf(Candidate, FireTick, Radius);
		if(Score > BestScore)
		{
			BestScore = Score;
			BestAngle = Angle;
			Best = Candidate;
		}
	}

	if(!Best.m_Hits)
		return false;

	// Then walk around the winner to find the middle of the band that works
	// rather than the edge the coarse step happened to land on. An aim that only
	// works at the edge is one packet of jitter away from missing.
	const float CoarseStep = 2.0f * pi / (float)Steps;
	for(int Refine = 1; Refine <= 6; Refine++)
	{
		const float Offset = CoarseStep * (float)Refine / 7.0f;
		const float aAngles[2] = {BestAngle - Offset, BestAngle + Offset};
		for(const float Angle : aAngles)
		{
			const CCandidate Candidate = Evaluate(Angle, FireTick, FirePos, MaxBounces, BounceTicks, Energy, BounceCost);
			if(!Candidate.m_Hits || Candidate.m_Margin < 2 || Candidate.m_Saved < MIN_SAVED_TICKS)
				continue;
			const float Score = ScoreOf(Candidate, FireTick, Radius);
			if(Score > BestScore)
			{
				BestScore = Score;
				Best = Candidate;
			}
		}
	}

	// Leave the winning path behind for the drawing.
	Evaluate(std::atan2((float)Best.m_TargetY, (float)Best.m_TargetX), FireTick, FirePos, MaxBounces, BounceTicks, Energy, BounceCost);
	m_vSolution = m_vSegments;

	m_HasSolution = true;
	m_Solution = Best;
	m_SolutionDir = normalize(vec2((float)Best.m_TargetX, (float)Best.m_TargetY));
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
	CCharacter *pPredicted = pGameClient->m_PredictedWorld.GetCharacterById(LocalId);
	// While already frozen there is nothing to look for: the shot cannot be taken
	// any more, it had to leave before the freeze started.
	if(pPredicted == nullptr || pPredicted->m_FreezeTime > 0)
	{
		m_HasSolution = false;
		m_vSolution.clear();
		m_Status = pPredicted != nullptr ? EStatus::TOO_LATE : EStatus::QUIET;
		RenderStatus();
		return;
	}

	if(!SelfHitPossible())
	{
		m_HasSolution = false;
		m_vSolution.clear();
		m_Status = EStatus::IMPOSSIBLE;
		RenderStatus();
		return;
	}

	const float Now = Client()->LocalTime();
	const float Interval = std::clamp(g_Config.m_ClUnfreezeInterval, 40, 500) / 1000.0f;
	if(Now - m_LastSearchTime >= Interval || Now < m_LastSearchTime)
	{
		m_LastSearchTime = Now;
		m_HasSolution = false;
		m_vSolution.clear();

		// Simulating further than the shot can ever reach is wasted work.
		const CTuningParams *pTuning = &GameClient()->m_aTuning[g_Config.m_ClDummy];
		const int BounceTicks = std::max(1, (int)(Client()->GameTickSpeed() * (int)pTuning->m_LaserBounceDelay / 1000) + 1);
		const int Reach = std::clamp(g_Config.m_ClUnfreezeBounces, 4, 40) * BounceTicks + 4;
		const int Horizon = std::min(g_Config.m_ClUnfreezeHorizon, Reach);

		m_FreezeAhead = Predict(LocalId, PredTick, Horizon);
		if(m_FreezeAhead)
			Search(PredTick + 1, pPredicted->Core()->m_Pos);
	}

	// Automatic mode fires as soon as it has a plan, then holds off until the shot
	// already on its way has arrived and the weapon has reloaded.
	if(g_Config.m_ClUnfreeze == 2 && m_HasSolution && !m_WantShot)
	{
		const int FireDelay = (int)(GameClient()->m_aTuning[g_Config.m_ClDummy].GetWeaponFireDelay(WEAPON_LASER) * Client()->GameTickSpeed());
		const bool Cooled = m_LastShotTick < 0 || PredTick - m_LastShotTick >= FireDelay;
		const bool Landed = PredTick >= m_ShotLandsTick;
		if(Cooled && Landed)
			m_WantShot = true;
	}

	if(m_HasSolution)
	{
		const bool HoldsLaser = pPredicted->GetActiveWeapon() == WEAPON_LASER;
		m_Status = HoldsLaser || g_Config.m_ClUnfreezeSwitchWeapon ? EStatus::READY : EStatus::NO_LASER;
	}
	else if(m_FreezeAhead)
	{
		m_Status = EStatus::SEARCHING;
	}

	RenderPlan();
	RenderStatus();
}

bool CUnfreeze::ApplyInput(CNetObj_PlayerInput *pInput)
{
	const int PredTick = Client()->PredGameTick(g_Config.m_ClDummy);

	// Handing the weapon back comes first, so the player is never left holding a
	// laser because a plan disappeared.
	if(m_RestoreWeapon >= 0 && m_SwitchSends <= 0)
	{
		pInput->m_WantedWeapon = m_RestoreWeapon;
		m_RestoreWeapon = -1;
		m_SwitchDeadlineTick = -1;
		return true;
	}

	if(!m_HasSolution || !m_WantShot)
	{
		// A switch started for a plan that has since vanished is wound back on
		// the next input.
		if(m_RestoreWeapon >= 0)
			m_SwitchSends = 0;
		return false;
	}

	const CCharacter *pPredicted = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_Snap.m_LocalClientId);
	if(pPredicted == nullptr)
		return false;

	// The predicted character knows which weapon is in hand a round trip before
	// the snapshot does, which is the difference between firing on the tick the
	// plan was made for and firing far too late.
	if(pPredicted->GetActiveWeapon() != WEAPON_LASER)
	{
		if(!g_Config.m_ClUnfreezeSwitchWeapon)
			return false;

		if(m_RestoreWeapon < 0)
		{
			// Remembered so the player gets their weapon back, including the zero
			// that means "whatever the wheel last picked".
			m_RestoreWeapon = pInput->m_WantedWeapon;
			m_SwitchDeadlineTick = PredTick + 50;
			// The server reads the wanted weapon out of two different copies of
			// the input, so one send is not reliably enough.
			m_SwitchSends = 3;
		}
		else if(PredTick > m_SwitchDeadlineTick)
		{
			// The server is not giving us the laser. Stop asking.
			m_WantShot = false;
			m_SwitchSends = 0;
			return false;
		}

		m_SwitchSends--;
		pInput->m_WantedWeapon = WEAPON_LASER + 1;
		return true;
	}

	m_WantShot = false;

	// An even step leaves the parity alone, so the player's own fire key stays in
	// step with the counter, and the laser's full automatic mode is not armed.
	pInput->m_Fire = (pInput->m_Fire + 2) & INPUT_STATE_MASK;
	m_FiredTargetX = m_Solution.m_TargetX;
	m_FiredTargetY = m_Solution.m_TargetY;
	m_LastShotTick = PredTick;
	m_AimUntilTick = PredTick + 3;
	m_ShotLandsTick = m_Solution.m_EvalTick + 1;
	// The weapon goes back on the next input, now that the shot is out.
	m_SwitchSends = 0;
	return true;
}

bool CUnfreeze::ApplyAim(CNetObj_PlayerInput *pInput) const
{
	// Keyed on the shot, not on the current plan: the aim that goes out has to be
	// the one the shot was fired with, even if the search has moved on since.
	if(Client()->PredGameTick(g_Config.m_ClDummy) > m_AimUntilTick)
		return false;
	if(m_FiredTargetX == 0 && m_FiredTargetY == 0)
		return false;

	pInput->m_TargetX = m_FiredTargetX;
	pInput->m_TargetY = m_FiredTargetY;
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
			if(Step.m_FreezeTime <= 0)
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
			// The stretch before the first wall can never come back to the tee, so
			// it is drawn faded to keep the useful part readable.
			Graphics()->SetColor(Segment.m_Bounces < 1 ? Color.WithMultipliedAlpha(0.4f) : Color);
			const IGraphics::CLineItem LineItem(Segment.m_From.x, Segment.m_From.y, Segment.m_To.x, Segment.m_To.y);
			Graphics()->LinesDraw(&LineItem, 1);
		}
		Graphics()->LinesEnd();

		Graphics()->QuadsBegin();
		Graphics()->SetColor(Color);
		const IGraphics::CQuadItem QuadItem(m_Solution.m_HitPos.x - 8.0f, m_Solution.m_HitPos.y - 8.0f, 16.0f, 16.0f);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}

	Graphics()->MapScreen(SavedScreenRect);
}

void CUnfreeze::RenderStatus() const
{
	if(!g_Config.m_ClUnfreezeShowStatus || m_Status == EStatus::QUIET)
		return;

	char aBuf[192];
	const char *pText = nullptr;
	ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
	switch(m_Status)
	{
	case EStatus::READY:
		str_format(aBuf, sizeof(aBuf), Localize("Unfreeze shot ready, saves %d ticks of freeze"), m_Solution.m_Saved);
		pText = aBuf;
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
	case EStatus::IMPOSSIBLE:
		pText = Localize("On this server a laser cannot hit the tee that fired it");
		Color = ColorRGBA(0.8f, 0.5f, 0.5f, 1.0f);
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
