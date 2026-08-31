#include "unfreeze.h"

#include <base/log.h>
#include <base/math.h>
#include <base/time.h>
#include <base/vmath.h>

#include <base/io.h>

#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/ui.h>
#include <game/collision.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <limits>

namespace {
// The aim is sent as whole units, so the angle that leaves the client is never
// exactly the angle that was wanted. Everything is therefore traced through the
// integer target, at a length long enough that the rounding is worth well under
// a tenth of a degree.
constexpr float AIM_RADIUS = 2000.0f;
// A hit that only shortens a freeze the tee was going to sit out anyway has to
// be worth something; two ticks of that is not. A stretch that ends because a
// tile freezes the tee again is different: those ticks are control handed back,
// and two of them are enough to hook or jump out.
constexpr int MIN_SAVED_NATURAL = 8;
constexpr int MIN_SAVED_TILE = 2;
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
	m_PlanFireTick = -1;
	m_LastValidateTick = -1;
	m_OutcomeTick = -1;
	m_FiredTargetX = 0;
	m_FiredTargetY = 0;
	m_RestoreWeapon = -1;
	m_WantLaser = false;
	m_SwitchDeadlineTick = -1;
	m_vSolution.clear();
	m_vFlight.clear();
	m_FirstFlightTick = -1;
	m_FirstUsefulTick = -1;
	m_LastUsefulTick = -1;
	m_FreezeAhead = false;
	m_WindowAhead = false;
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

int CUnfreeze::FreezeSaved(int Tick, bool *pTileEnd) const
{
	*pTileEnd = false;
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
		if(pAt == nullptr)
			return i - Tick;
		if(pAt->m_FreezeTime <= 0 || pAt->m_OnFreeze)
		{
			*pTileEnd = pAt->m_FreezeTime > 0 && pAt->m_OnFreeze;
			return i - Tick;
		}
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

bool CUnfreeze::HoldsLaser() const
{
	const CCharacter *pPredicted = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_Snap.m_LocalClientId);
	if(pPredicted != nullptr && pPredicted->GetActiveWeapon() == WEAPON_LASER)
		return true;
	// The prediction only believes in weapons it has been told about, so on a
	// server that sends no extended character it will not model the switch at
	// all. What the server says the tee is holding is then the only signal.
	const CNetObj_Character *pSnap = GameClient()->m_Snap.m_pLocalCharacter;
	return pSnap != nullptr && pSnap->m_Weapon == WEAPON_LASER;
}

// Whether the player owns the laser, when that can be known. A DDNet server
// puts the whole inventory in the extended character object, and the core reads
// it; without one, only the weapon in hand is ever known and asking is the only
// way to find out.
float CUnfreeze::StepAt(int Tick) const
{
	const CFlightStep *pStep = FlightAt(Tick);
	return pStep == nullptr ? 0.0f : distance(pStep->m_PrevPos, pStep->m_Pos);
}

float CUnfreeze::FastestInWindow() const
{
	float Fastest = 0.0f;
	for(int Tick = m_FirstUsefulTick; Tick >= 0 && Tick <= m_LastUsefulTick; Tick++)
		Fastest = std::max(Fastest, StepAt(Tick));
	return Fastest;
}

bool CUnfreeze::OwnsLaser() const
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_aCharacters[LocalId].m_HasExtendedData)
		return true;
	return (GameClient()->m_Snap.m_aCharacters[LocalId].m_ExtendedData.m_Flags & CHARACTERFLAG_WEAPON_LASER) != 0;
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

	m_HasUsefulBox = false;
	for(int i = 0; i < (int)m_vFlight.size(); i++)
	{
		const int Tick = m_FirstFlightTick + i;
		if(!IsUsefulTick(Tick))
			continue;
		if(m_FirstUsefulTick < 0)
			m_FirstUsefulTick = Tick;
		m_LastUsefulTick = Tick;

		const vec2 &Pos = m_vFlight[i].m_Pos;
		if(!m_HasUsefulBox)
		{
			m_HasUsefulBox = true;
			m_UsefulMin = Pos;
			m_UsefulMax = Pos;
		}
		else
		{
			m_UsefulMin.x = std::min(m_UsefulMin.x, Pos.x);
			m_UsefulMin.y = std::min(m_UsefulMin.y, Pos.y);
			m_UsefulMax.x = std::max(m_UsefulMax.x, Pos.x);
			m_UsefulMax.y = std::max(m_UsefulMax.y, Pos.y);
		}
	}
	if(m_HasUsefulBox)
	{
		const float Radius = CCharacterCore::PhysicalSize();
		m_UsefulMin -= vec2(Radius, Radius);
		m_UsefulMax += vec2(Radius, Radius);
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

		// Whatever is left of the beam has to cover at least the straight line to
		// the ticks worth hitting, so anything shorter than that is finished.
		if(m_HasUsefulBox)
		{
			const float dx = std::max({m_UsefulMin.x - Pos.x, 0.0f, Pos.x - m_UsefulMax.x});
			const float dy = std::max({m_UsefulMin.y - Pos.y, 0.0f, Pos.y - m_UsefulMax.y});
			if(std::sqrt(dx * dx + dy * dy) > Energy)
				break;
		}
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
	// How near the beam came to a tick worth hitting, even on angles that never
	// reach one. The sweep ranks angles by this, which is what lets it look at a
	// few dozen angles closely instead of all of them.
	float Approach = std::numeric_limits<float>::max();
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

		// Whoever the beam reaches first ends it. The owner only counts from the
		// first bounce on, so work out how far along the beam it would be hit and
		// let another tee take the shot only if it stands closer than that.
		float OwnerAlong = std::numeric_limits<float>::max();
		float Miss = 0.0f;
		vec2 Closest;
		const bool OwnerReachable = Segment.m_Bounces >= 1 &&
					    closest_point_on_line(Segment.m_From, Segment.m_To, pStep->m_Pos, Closest);
		if(OwnerReachable)
		{
			Miss = distance(pStep->m_Pos, Closest);
			if(IsUsefulTick(Segment.m_EvalTick - 1))
				Approach = std::min(Approach, Miss);
			if(Miss < Radius)
				OwnerAlong = distance(Segment.m_From, Closest);
		}

		bool Blocked = false;
		for(int i = 0; i < pStep->m_NumOthers; i++)
		{
			vec2 OtherClosest;
			if(!closest_point_on_line(Segment.m_From, Segment.m_To, pStep->m_aOthers[i], OtherClosest))
				continue;
			if(distance(pStep->m_aOthers[i], OtherClosest) >= Radius)
				continue;
			if(distance(Segment.m_From, OtherClosest) < OwnerAlong)
			{
				Blocked = true;
				break;
			}
		}
		if(Blocked)
			break;

		if(OwnerAlong == std::numeric_limits<float>::max())
			continue;

		// This is where the shot ends, whatever it is worth. If the tick is no
		// good the angle is dead, because everything after it never happens.
		if(!IsUsefulTick(Segment.m_EvalTick - 1))
			break;

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
		Candidate.m_FireTick = FireTick;
		Candidate.m_Saved = FreezeSaved(Segment.m_EvalTick - 1, &Candidate.m_TileEnd);
		Candidate.m_Margin = Margin;
		Candidate.m_Miss = Miss;
		Candidate.m_HitPos = pStep->m_Pos;
		break;
	}

	Candidate.m_Approach = Approach;
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
	// A window that ends on a tile is short by nature, so its length says little
	// about its worth; what it buys is the tee's control back.
	const float Worth = Candidate.m_TileEnd ? (float)Candidate.m_Saved + 6.0f : (float)Candidate.m_Saved;
	return Worth +
	       8.0f * (float)(Candidate.m_Margin - 1) +
	       10.0f * Centred -
	       0.1f * (float)(Candidate.m_EvalTick - FireTick);
}

// Which fire delays are worth tracing. A bounce lands every BounceTicks, so the
// ticks a shot fired now can reach are one residue class in eight; firing a tick
// later moves the whole schedule. Every tick worth hitting names exactly one
// delay that reaches it, so the list is short and costs nothing to work out.
int CUnfreeze::UsefulDelays(int PredTick, int BounceTicks, int MinDelay, int MaxDelay, bool *pDelays, int MaxDelays) const
{
	for(int i = 0; i < MaxDelays; i++)
		pDelays[i] = false;

	int Count = 0;
	const int Limit = std::min(MaxDelay, MaxDelays);
	for(int Tick = m_FirstUsefulTick; Tick >= 0 && Tick <= m_LastUsefulTick; Tick++)
	{
		if(!IsUsefulTick(Tick))
			continue;
		const int Residue = ((Tick - PredTick + 1) % BounceTicks + BounceTicks) % BounceTicks;
		// The same tick is reachable by firing now, or by firing a whole bounce
		// later with one bounce fewer. That second one used to be left out, and
		// it is the only one left when the shot cannot leave for a few ticks.
		for(int Delay = Residue; Delay < Limit; Delay += BounceTicks)
		{
			if(Delay < MinDelay || pDelays[Delay])
				continue;
			// The bounce has to be a real one: the stretch before the first wall
			// can never touch the tee that fired it.
			if(Tick < PredTick + Delay - 1 + BounceTicks)
				continue;
			// And the tee has to still be able to shoot on that tick.
			if(Delay > 0)
			{
				const CFlightStep *pAt = FlightAt(PredTick + Delay);
				if(pAt == nullptr || pAt->m_FreezeTime > 0)
					continue;
			}
			pDelays[Delay] = true;
			Count++;
		}
	}
	return Count;
}

bool CUnfreeze::Search(int FireTick, vec2 FirePos, CCandidate &Best, float &BestScore, std::vector<CSegment> &vBestPath, int64_t Deadline)
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

	// Tracing every angle at the resolution the setting asks for is nearly all of
	// what this module costs, and nearly all of that is spent on angles that send
	// the beam nowhere near the flight. So the sweep is coarse, and only the few
	// angles that came closest are then looked at closely. The fine resolution is
	// still reached, on the angles where it changes the answer.
	const int Coarse = std::max(60, Steps / 6);
	const float CoarseStep = 2.0f * pi / (float)Coarse;

	bool Improved = false;
	int Since = 0;
	// Checked every so often rather than every angle, because reading the clock
	// costs about as much as tracing one.
	const auto OutOfTime = [&]() {
		if(++Since < 32)
			return false;
		Since = 0;
		return time_get_impl() > Deadline;
	};

	const auto Try = [&](float Angle, float *pRank) {
		const CCandidate Candidate = Evaluate(Angle, FireTick, FirePos, MaxBounces, BounceTicks, Energy, BounceCost);
		const bool Good = Candidate.m_Hits && Candidate.m_Saved >= (Candidate.m_TileEnd ? MIN_SAVED_TILE : MIN_SAVED_NATURAL);
		if(Good)
		{
			const float Score = ScoreOf(Candidate, FireTick, Radius);
			if(Score > BestScore)
			{
				BestScore = Score;
				Best = Candidate;
				vBestPath = m_vSegments;
				Improved = true;
			}
		}
		// A hit sorts ahead of every miss, and among misses the nearest first. A
		// hit is ranked by how far from the middle of the tee it passes, so that
		// the walk below has something to descend even once it is landing: the
		// middle of a band is what survives the flight being slightly off.
		*pRank = Good ? Candidate.m_Miss - Radius : Candidate.m_Approach;
		m_BestApproach = std::min(m_BestApproach, Candidate.m_Approach);
		return Good;
	};

	// Tracing every angle at the resolution the setting asks for is nearly all of
	// what this module costs, and nearly all of that is spent on angles that send
	// the beam nowhere near the flight. So the sweep is coarse, and only the few
	// angles that came closest are looked at closely, twice: once to find the
	// band, once to find the middle of it. That reaches an angle finer than a
	// plain sweep of the same cost, which matters because a band wide enough to
	// hit a tee across a room can still be a quarter of a degree.
	m_vProbes.clear();
	for(int Step = 0; Step < Coarse; Step++)
	{
		if(OutOfTime())
			break;
		const float Angle = (float)Step * CoarseStep;
		float Rank = 0.0f;
		const bool Good = Try(Angle, &Rank);
		if(Good || Rank < 4.0f * Radius)
			m_vProbes.emplace_back(Angle, Rank);
	}

	constexpr int MAX_REFINED = 8;
	constexpr int MAX_DEEP = 2;
	const auto KeepBest = [&](size_t Count) {
		if(m_vProbes.size() <= Count)
			return;
		std::partial_sort(m_vProbes.begin(), m_vProbes.begin() + Count, m_vProbes.end(),
			[](const std::pair<float, float> &Left, const std::pair<float, float> &Right) { return Left.second < Right.second; });
		m_vProbes.resize(Count);
	};

	KeepBest(MAX_REFINED);
	std::vector<std::pair<float, float>> vDeep;
	for(const auto &[Around, Rank] : m_vProbes)
	{
		for(int Refine = -8; Refine <= 8; Refine++)
		{
			if(Refine == 0)
				continue;
			if(OutOfTime())
			{
				vDeep.clear();
				return Improved;
			}
			const float Angle = Around + CoarseStep * (float)Refine / 9.0f;
			float SubRank = 0.0f;
			const bool Good = Try(Angle, &SubRank);
			if(Good || SubRank < Radius * 2.0f)
				vDeep.emplace_back(Angle, SubRank);
		}
	}

	// The narrowest bands lie between two of those, and sampling ever finer is a
	// poor way to find them: a band a sixteenth of a degree wide needs a grid of
	// thousands of angles to be stumbled on. How near the beam passes is a smooth
	// function of the angle though, so the best few probes are walked downhill on
	// it instead. A dozen traces then land in the middle of a band that no grid of
	// this cost would have hit at all.
	m_vProbes = vDeep;
	KeepBest(MAX_DEEP);
	for(const auto &[Around, Rank] : m_vProbes)
	{
		// Bracket the probe a fifth of a coarse step either side and close in on
		// the nearest pass. Golden section, so every step but the first reuses one
		// of the two angles already traced.
		constexpr float GOLDEN = 0.61803399f;
		float Low = Around - CoarseStep * 0.2f;
		float High = Around + CoarseStep * 0.2f;
		float LeftAngle = High - GOLDEN * (High - Low);
		float RightAngle = Low + GOLDEN * (High - Low);
		float LeftRank = 0.0f, RightRank = 0.0f;
		if(OutOfTime())
			return Improved;
		Try(LeftAngle, &LeftRank);
		if(OutOfTime())
			return Improved;
		Try(RightAngle, &RightRank);
		for(int Step = 0; Step < 12; Step++)
		{
			if(OutOfTime())
				return Improved;
			if(LeftRank < RightRank)
			{
				High = RightAngle;
				RightAngle = LeftAngle;
				RightRank = LeftRank;
				LeftAngle = High - GOLDEN * (High - Low);
				Try(LeftAngle, &LeftRank);
			}
			else
			{
				Low = LeftAngle;
				LeftAngle = RightAngle;
				LeftRank = RightRank;
				RightAngle = Low + GOLDEN * (High - Low);
				Try(RightAngle, &RightRank);
			}
			// Below what one unit of the aim on the wire can express there is
			// nothing left to find.
			if(High - Low < 1.0f / AIM_RADIUS)
				break;
		}
	}

	return Improved;
}

// A committed plan is not searched over again, or a search that finds nothing
// would throw it away. But it still has to be true: the flight it was made from
// is a prediction, and by the tick the shot is due the tee may not be where it
// was going to be. So the flight is predicted afresh and the one angle already
// chosen is traced again. That is a single Evaluate rather than a whole sweep.
bool CUnfreeze::StillHolds(int LocalId, int PredTick)
{
	const CTuningParams *pTuning = &GameClient()->m_aTuning[g_Config.m_ClDummy];
	const int BounceTicks = std::clamp((int)(Client()->GameTickSpeed() * (int)pTuning->m_LaserBounceDelay / 1000) + 1, 1, 16);
	const int Reach = std::clamp(g_Config.m_ClUnfreezeBounces, 4, 40) * BounceTicks + 4;
	const int Horizon = std::min(g_Config.m_ClUnfreezeHorizon, Reach);
	if(!Predict(LocalId, PredTick, Horizon))
		return false;

	const int Delay = m_PlanFireTick - 1 - PredTick;
	const CFlightStep *pAt = Delay > 0 ? FlightAt(PredTick + Delay) : nullptr;
	if(Delay > 0 && pAt == nullptr)
		return false;
	CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(LocalId);
	if(Delay <= 0 && pChar == nullptr)
		return false;
	const vec2 FirePos = Delay > 0 ? pAt->m_Pos : pChar->Core()->m_Pos;

	const CTuningParams *pLaser = LaserTuning(FirePos);
	const int MaxBounces = BounceBudget(m_PlanFireTick, BounceTicks, pLaser->m_LaserBounceNum);
	if(MaxBounces < 1)
		return false;

	const CCandidate Now = Evaluate(std::atan2(m_SolutionDir.y, m_SolutionDir.x), m_PlanFireTick, FirePos,
		MaxBounces, BounceTicks, ReachTuning(FirePos)->m_LaserReach, pLaser->m_LaserBounceCost);
	if(!Now.m_Hits || Now.m_EvalTick != m_Solution.m_EvalTick)
		return false;

	// The flight moved a little; aim at where the tee is going to be now.
	m_Solution = Now;
	m_vSolution = m_vSegments;
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
		Debug("idle: no local character (id %d, snap %d, spectating %d)",
			LocalId, pLocal != nullptr, (int)pGameClient->m_Snap.m_SpecInfo.m_Active);
		Reset();
		return;
	}

	const int PredTick = Client()->PredGameTick(g_Config.m_ClDummy);
	CCharacter *pPredicted = pGameClient->m_PredictedWorld.GetCharacterById(LocalId);

	// What became of the last shot. Waited out a few ticks past the moment it was
	// due, so that the server's answer has arrived rather than the prediction's.
	if(m_OutcomeTick >= 0 && PredTick >= m_OutcomeTick + 4)
	{
		const int Now = pPredicted != nullptr ? pPredicted->m_FreezeTime : -1;
		// Four ticks of freeze run out by themselves; anything more than that came
		// off because the shot landed.
		const int Dropped = m_OutcomeFreezeAtFire - Now;
		Debug("OUTCOME: %s. freeze was %d at the shot, %d now, so %d ticks came off, %d were promised",
			Now == 0 || Dropped > 8 ? "the shot landed" : "nothing came off, the shot missed",
			m_OutcomeFreezeAtFire, Now, Dropped, m_OutcomeSaved);
		m_OutcomeTick = -1;
	}
	// While already frozen there is nothing to look for: the shot cannot be taken
	// any more, it had to leave before the freeze started.
	if(pPredicted == nullptr || pPredicted->m_FreezeTime > 0)
	{
		if(pPredicted == nullptr)
			Debug("idle: the predicted world has no character, is cl_predict on?");
		else
			Debug("too late: already frozen for %d more ticks", pPredicted->m_FreezeTime);
		m_HasSolution = false;
		m_WindowAhead = false;
		m_vSolution.clear();
		m_Status = pPredicted != nullptr ? EStatus::TOO_LATE : EStatus::QUIET;
		RenderStatus();
		return;
	}

	// A shot that needs a weapon the player does not have is not worth searching
	// for, and asking for it would only take their weapon wheel away for the
	// whole window while nothing arrives.
	if(!OwnsLaser())
	{
		Debug("no laser owned: the extended character says it is not in the inventory");
		m_HasSolution = false;
		m_WantShot = false;
		m_WindowAhead = false;
		m_vSolution.clear();
		m_Status = EStatus::NO_LASER;
		RenderStatus();
		return;
	}

	if(!SelfHitPossible())
	{
		Debug("impossible: old laser %d, ddrace prediction %d",
			(int)GameClient()->m_PredictedWorld.m_WorldConfig.m_OldLaser,
			(int)GameClient()->m_PredictedWorld.m_WorldConfig.m_IsDDRace);
		m_HasSolution = false;
		m_vSolution.clear();
		m_Status = EStatus::IMPOSSIBLE;
		RenderStatus();
		return;
	}

	const float Now = Client()->LocalTime();
	const float Interval = std::clamp(g_Config.m_ClUnfreezeInterval, 40, 500) / 1000.0f;
	// A shot that has been decided on is left alone until it has been taken or
	// its tick has gone by. The search runs several times more often than the
	// lead a plan needs, and a search that finds nothing used to throw the
	// decided shot away a tick or two after it was armed. Against a real server
	// that was 160 shots armed and 3 fired.
	bool Committed = m_WantShot && m_HasSolution && m_PlanFireTick >= 0 && PredTick <= m_PlanFireTick;
	// Once a tick while it waits, the plan is held up against a fresh prediction.
	// A tee flung sideways covers more than its own width in a tick, so a plan
	// made ten ticks ago can be aimed at thin air by the time it fires.
	if(Committed && PredTick != m_LastValidateTick)
	{
		m_LastValidateTick = PredTick;
		if(!StillHolds(LocalId, PredTick))
		{
			Debug("plan went stale: the flight moved, looking again");
			m_HasSolution = false;
			m_WantShot = false;
			m_vSolution.clear();
			m_LastSearchTime = -1000.0f;
			Committed = false;
		}
	}
	if(!Committed && (Now - m_LastSearchTime >= Interval || Now < m_LastSearchTime))
	{
		m_LastSearchTime = Now;
		m_HasSolution = false;
		// Whatever was wanted was wanted for the plan that is being replaced.
		m_WantShot = false;
		m_vSolution.clear();

		// Whatever the settings ask for, the search may not cost more than this,
		// and it keeps the best it had when the time runs out. Without it a heavy
		// setting is felt as a stutter every time a freeze comes into range.
		const int64_t Started = time_get_impl();
		const int64_t Deadline = Started + (int64_t)std::clamp(g_Config.m_ClUnfreezeBudget, 1, 20) * time_freq() / 1000;

		// Simulating further than the shot can ever reach is wasted work.
		const CTuningParams *pTuning = &GameClient()->m_aTuning[g_Config.m_ClDummy];
		const int BounceTicks = std::clamp((int)(Client()->GameTickSpeed() * (int)pTuning->m_LaserBounceDelay / 1000) + 1, 1, 16);
		const int Reach = std::clamp(g_Config.m_ClUnfreezeBounces, 4, 40) * BounceTicks + 4;
		const int Horizon = std::min(g_Config.m_ClUnfreezeHorizon, Reach);

		m_FreezeAhead = Predict(LocalId, PredTick, Horizon);
		// Reaching for the laser is worth starting the moment a stretch worth
		// shooting into is coming, rather than once an angle has been found for
		// it. Waiting for the plan costs the switch's ticks out of the few the
		// freeze leaves, and measured against a real server that is most of the
		// difference between a shot that leaves and one that is still being
		// prepared when the tee freezes.
		m_WindowAhead = m_FreezeAhead && m_FirstUsefulTick >= 0;
		if(m_WindowAhead && m_WantLaser)
			m_SwitchDeadlineTick = PredTick + 50;
		if(!m_FreezeAhead)
			Debug("quiet: no freeze within %d ticks", Horizon);
		if(m_FreezeAhead)
		{
			// Firing a tick later moves every bounce with it, so the shot is not
			// limited to the ticks a shot fired right now happens to land on.
			// Only the delays that could reach a tick worth hitting are traced.
			//
			// The earliest of them is not zero. The search runs on a render frame,
			// the shot can only leave on an input that has not gone out yet, and a
			// laser that is not in hand costs a few ticks to switch to. A plan made
			// for a tick that has already passed is dropped unfired, which is what
			// made the module look like it did nothing at all.
			constexpr int MAX_DELAYS = 16;
			constexpr int MAX_SWEPT = 6;
			const bool InHand = HoldsLaser();
			const int MinDelay = InHand || !g_Config.m_ClUnfreezeSwitchWeapon ? 2 : 8;
			bool aDelays[MAX_DELAYS] = {};
			const int NumDelays = UsefulDelays(PredTick, BounceTicks, MinDelay, std::min(2 * BounceTicks, MAX_DELAYS), aDelays, MAX_DELAYS);
			CCandidate Best;
			float BestScore = 0.0f;
			m_BestApproach = std::numeric_limits<float>::max();
			std::vector<CSegment> vBestPath;
			// Which delays to trace has to be settled before any of them is
			// traced, because the budget is then split evenly between them. Spent
			// first come first served, the earliest delays eat all of it and the
			// later ones are never looked at, and the shot is often in one of
			// those: a bounce ladder that starts later can be the only one that
			// puts a bounce inside the window.
			int aSwept[MAX_SWEPT];
			int Count = 0;
			for(int Delay = 0; Delay < MAX_DELAYS && NumDelays > 0 && Count < MAX_SWEPT; Delay++)
			{
				if(!aDelays[Delay])
					continue;
				if(Delay > 0 && FlightAt(PredTick + Delay) == nullptr)
					continue;
				aSwept[Count++] = Delay;
			}
			const int64_t Slice = Count > 0 ? (Deadline - Started) / Count : 0;
			for(int i = 0; i < Count; i++)
			{
				const int Delay = aSwept[i];
				const CFlightStep *pAt = Delay > 0 ? FlightAt(PredTick + Delay) : nullptr;
				const vec2 FirePos = Delay > 0 ? pAt->m_Pos : pPredicted->Core()->m_Pos;
				Search(PredTick + 1 + Delay, FirePos, Best, BestScore, vBestPath, Started + Slice * (i + 1));
			}

			if(Best.m_Hits)
			{
				m_HasSolution = true;
				m_Solution = Best;
				m_PlanFireTick = Best.m_FireTick;
				m_SolutionDir = normalize(vec2((float)Best.m_TargetX, (float)Best.m_TargetY));
				m_vSolution = vBestPath;
				Debug("plan: fire on tick %d (+%d), hit +%d, saves %d ticks, margin %d, tile end %d, step at the hit %.0f, fastest in window %.0f",
					Best.m_FireTick, Best.m_FireTick - PredTick, Best.m_EvalTick - 1 - PredTick,
					Best.m_Saved, Best.m_Margin, (int)Best.m_TileEnd,
					StepAt(Best.m_EvalTick - 1), FastestInWindow());
			}
			else
			{
				Debug("no shot: window +%d..+%d (%d ticks), %d delays, laser %d, closest the beam came %.0f, fastest in window %.0f, slowest %.0f",
					m_FirstUsefulTick - PredTick, m_LastUsefulTick - PredTick, m_LastUsefulTick - m_FirstUsefulTick + 1,
					NumDelays, (int)InHand,
					m_BestApproach == std::numeric_limits<float>::max() ? -1.0f : m_BestApproach,
					FastestInWindow(), StepAt(m_FirstUsefulTick));
			}
		}

		m_LastSearchMs = (float)((time_get_impl() - Started) * 1000.0 / (double)time_freq());
	}

	// Automatic mode fires as soon as it has a plan, then holds off until the shot
	// already on its way has arrived and the weapon has reloaded.
	if(g_Config.m_ClUnfreeze == 2 && m_HasSolution && !m_WantShot)
	{
		const int FireDelay = (int)(GameClient()->m_aTuning[g_Config.m_ClDummy].GetWeaponFireDelay(WEAPON_LASER) * Client()->GameTickSpeed());
		const bool Cooled = m_LastShotTick < 0 || PredTick - m_LastShotTick >= FireDelay;
		const bool Landed = PredTick >= m_ShotLandsTick;
		if(Cooled && Landed)
		{
			m_WantShot = true;
			Debug("armed: taking the shot planned for tick %d", m_PlanFireTick);
		}
		else
			Debug("holding off: reloaded %d, previous shot landed %d", (int)Cooled, (int)Landed);
	}

	if(m_HasSolution)
	{
		m_Status = HoldsLaser() || g_Config.m_ClUnfreezeSwitchWeapon ? EStatus::READY : EStatus::NO_LASER;
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
	if(!m_HasSolution || !m_WantShot)
	{
		if(m_WantShot)
			Debug("shot wanted but the plan is gone");
		return false;
	}

	const CCharacter *pPredicted = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_Snap.m_LocalClientId);
	if(pPredicted == nullptr)
	{
		Debug("shot wanted but there is no predicted character to fire it");
		return false;
	}

	// The input being built here is the one the server runs on the tick that
	// PredGameTick names right now, not the one after it. Getting that wrong
	// means the plan's tick is never the tick that arrives, and the shot is
	// dropped as stale every single time without ever being fired.
	const int PredTick = Client()->PredGameTick(g_Config.m_ClDummy);

	// The predicted character knows which weapon is in hand a round trip before
	// the snapshot does, and it runs the same switch code as the server.
	if(!HoldsLaser())
	{
		Debug("waiting for the laser, weapon %d predicted, %d in the snapshot, plan tick %d, now %d",
			pPredicted->GetActiveWeapon(),
			GameClient()->m_Snap.m_pLocalCharacter != nullptr ? GameClient()->m_Snap.m_pLocalCharacter->m_Weapon : -1,
			m_PlanFireTick, PredTick);
		if(m_PlanFireTick >= 0 && PredTick > m_PlanFireTick)
		{
			// The laser did not arrive in time. Drop the plan and look again at
			// once rather than sitting out the rest of the interval.
			m_WantShot = false;
			m_HasSolution = false;
			m_LastSearchTime = -1000.0f;
		}
		return false;
	}

	if(m_PlanFireTick >= 0)
	{
		if(PredTick < m_PlanFireTick)
			return false;
		// A plan belongs to one tick. The predicted tick can skip one when a
		// frame runs long or the ping jumps, and then the aim points at where the
		// tee was going to be at another moment, so the plan is thrown away and a
		// new one is looked for straight away.
		if(PredTick > m_PlanFireTick)
		{
			Debug("plan missed: it was for tick %d and the input is for %d, looking again",
				m_PlanFireTick, PredTick);
			m_WantShot = false;
			m_HasSolution = false;
			m_LastSearchTime = -1000.0f;
			return false;
		}
	}

	m_WantShot = false;
	m_OutcomeTick = m_Solution.m_EvalTick - 1;
	m_OutcomeFreezeAtFire = pPredicted->m_FreezeTime;
	m_OutcomeSaved = m_Solution.m_Saved;
	Debug("SHOT on tick %d, aim %d,%d, expecting the hit on +%d and %d ticks off",
		PredTick, m_Solution.m_TargetX, m_Solution.m_TargetY, m_OutcomeTick - PredTick, m_Solution.m_Saved);

	// An even step leaves the parity alone, so the player's own fire key stays in
	// step with the counter, and the laser's full automatic mode is not armed.
	pInput->m_Fire = (pInput->m_Fire + 2) & INPUT_STATE_MASK;
	m_FiredTargetX = m_Solution.m_TargetX;
	m_FiredTargetY = m_Solution.m_TargetY;
	m_LastShotTick = PredTick;
	m_AimUntilTick = PredTick + 3;
	m_ShotLandsTick = m_Solution.m_EvalTick + 1;
	// The shot is out, so the laser is no longer wanted and the player's weapon
	// goes back from the next send on.
	m_WantLaser = false;
	return true;
}

bool CUnfreeze::ApplyWeapon(CNetObj_PlayerInput *pSendData)
{
	// Not const: asking the predicted character which weapons it has is not a
	// const call there.
	CCharacter *pPredicted = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_Snap.m_LocalClientId);
	if(pPredicted == nullptr)
	{
		m_WantLaser = false;
		m_RestoreWeapon = -1;
		return false;
	}
	const int PredTick = Client()->PredGameTick(g_Config.m_ClDummy);

	// Handing the weapon back has to come first and has to keep going until the
	// player is really holding it again, or a plan that vanished mid switch
	// leaves them on the laser.
	if(!m_WantLaser && m_RestoreWeapon >= 0)
	{
		if(pPredicted->GetActiveWeapon() == m_RestoreWeapon)
		{
			m_RestoreWeapon = -1;
			return false;
		}
		pSendData->m_WantedWeapon = m_RestoreWeapon + 1;
		return true;
	}

	const bool Wanted = (m_HasSolution && m_WantShot) || m_WindowAhead;
	if(!Wanted || g_Config.m_ClUnfreeze == 0 || !g_Config.m_ClUnfreezeSwitchWeapon)
	{
		m_WantLaser = false;
		return false;
	}

	if(HoldsLaser())
	{
		// Already in hand. The field still goes out while the shot is pending,
		// because the server reads the request from one tick and the value from
		// the one before it, and a gap would let the switch lapse.
		if(m_WantLaser)
		{
			pSendData->m_WantedWeapon = WEAPON_LASER + 1;
			return true;
		}
		return false;
	}

	// Whether the player owns the laser at all cannot be known here: a snapshot
	// carries the weapon in hand and nothing else, so the predicted inventory
	// says no to every weapon that is not already held. Asking and waiting for
	// the deadline is the only way to find out. A ninja is different, that one is
	// visible and no switch will ever come out of it.
	if(pPredicted->GetActiveWeapon() == WEAPON_NINJA)
	{
		Debug("ninja in hand, no switch is possible");
		m_WantShot = false;
		m_WantLaser = false;
		return false;
	}

	if(!m_WantLaser)
	{
		m_WantLaser = true;
		// The weapon itself, not the field the input happened to carry.
		m_RestoreWeapon = pPredicted->GetActiveWeapon();
		m_SwitchDeadlineTick = PredTick + 50;
		Debug("asking for the laser, putting weapon %d back afterwards", m_RestoreWeapon);
	}
	else if(PredTick > m_SwitchDeadlineTick)
	{
		// The server is not giving us the laser. Stop asking.
		Debug("gave up asking for the laser after 50 ticks");
		m_WantShot = false;
		m_WantLaser = false;
		return false;
	}

	pSendData->m_WantedWeapon = WEAPON_LASER + 1;
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

void CUnfreeze::Debug(const char *pFormat, ...) const
{
	if(!g_Config.m_ClUnfreezeDebug)
		return;

	char aBuf[256];
	va_list Args;
	va_start(Args, pFormat);
	str_format_v(aBuf, sizeof(aBuf), pFormat, Args);
	va_end(Args);

	// The same line every frame would drown the interesting ones, so a line only
	// repeats when something else has been said in between or a second has gone by.
	const int Tick = Client()->PredGameTick(g_Config.m_ClDummy);
	if(str_comp(aBuf, m_aLastDebug) == 0 && Tick - m_LastDebugTick < Client()->GameTickSpeed())
		return;
	str_copy(m_aLastDebug, aBuf, sizeof(m_aLastDebug));
	m_LastDebugTick = Tick;

	IOHANDLE File = Storage()->OpenFile("unfreeze.log", IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
		return;
	char aLine[320];
	str_format(aLine, sizeof(aLine), "tick %d: %s", Tick, aBuf);
	const char Newline = 10;
	io_write(File, aLine, str_length(aLine));
	io_write(File, &Newline, 1);
	io_close(File);
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
		str_format(aBuf, sizeof(aBuf), Localize("Unfreeze shot ready, saves %d ticks of freeze (%.1f ms)"), m_Solution.m_Saved, m_LastSearchMs);
		pText = aBuf;
		Color = ColorRGBA(0.35f, 0.9f, 0.45f, 1.0f);
		break;
	case EStatus::SEARCHING:
		str_format(aBuf, sizeof(aBuf), Localize("Freeze ahead, no shot found (%.1f ms)"), m_LastSearchMs);
		pText = aBuf;
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
