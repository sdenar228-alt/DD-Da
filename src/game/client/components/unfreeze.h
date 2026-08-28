#ifndef GAME_CLIENT_COMPONENTS_UNFREEZE_H
#define GAME_CLIENT_COMPONENTS_UNFREEZE_H

#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>
#include <game/client/prediction/gameworld.h>

#include <generated/protocol.h>

#include <vector>

// Works out the shot that unfreezes you with your own laser.
//
// The shot has to be taken before the freeze, because a frozen tee cannot fire,
// and it can only come back to its owner after it has bounced once, which is
// eight ticks later at the default bounce delay. So the module predicts where
// the tee will be carried while it is frozen and searches the aim angles for one
// whose bounce path crosses that flight at a tick where the tee is frozen and no
// longer touching freeze tiles. Hitting it anywhere else is pointless: a tee
// unfrozen on a freeze tile is frozen again in the same tick, for the full
// duration, so it would be worse off than before.
class CUnfreeze : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnRender() override;
	void OnReset() override;

	// Both are called from CControls::SnapInput. The shot is a counter bump on
	// the stored input, so that the client's own fire bookkeeping stays in step;
	// the aim may only go on the send copy, never on the stored input.
	bool ApplyInput(CNetObj_PlayerInput *pInput);
	bool ApplyAim(CNetObj_PlayerInput *pInput) const;

private:
	// One predicted tick of the tee's flight.
	class CFlightStep
	{
	public:
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_PrevPos = vec2(0.0f, 0.0f);
		bool m_Frozen = false;
		// Deep freeze cannot be lifted by a laser at all.
		bool m_Deep = false;
		// True while a tile would freeze the tee again on this tick.
		bool m_OnFreeze = false;
	};

	// One stretch of the laser's flight, the way CLaser casts it.
	class CSegment
	{
	public:
		vec2 m_From = vec2(0.0f, 0.0f);
		vec2 m_To = vec2(0.0f, 0.0f);
		// The tick this stretch is tested against the players on.
		int m_Tick = 0;
		// How many walls the shot has hit before this stretch. It cannot touch
		// its owner while this is zero.
		int m_Bounces = 0;
	};

	// The world the flight is simulated in, kept as a member because copying one
	// allocates every entity in it.
	CGameWorld m_ScratchWorld;
	bool m_ScratchWorldUsed = false;

	std::vector<CFlightStep> m_vFlight;
	// The tick m_vFlight[0] belongs to.
	int m_FirstFlightTick = -1;
	// The last tick worth hitting. Following the shot past it is wasted work, and
	// the shot has to travel a long way to reach even that.
	int m_LastUsefulTick = -1;
	// True while the flight runs into a freeze that a shot could still help with.
	bool m_FreezeAhead = false;

	std::vector<CSegment> m_vSegments;
	std::vector<CSegment> m_vSolution;

	bool m_HasSolution = false;
	vec2 m_SolutionDir = vec2(1.0f, 0.0f);
	vec2 m_SolutionHitPos = vec2(0.0f, 0.0f);
	int m_SolutionHitTick = -1;
	// How many of the neighbouring ticks the shot would hit as well. A shot that
	// only lands on one exact tick is one lost packet away from missing.
	int m_SolutionMargin = 0;

	// Set by the console command or by the automatic mode, consumed by the next
	// input that is actually sent.
	bool m_WantShot = false;
	// The aim override stays on for a few sends: a lost input message makes the
	// server count the press when the next one arrives, and that one has to carry
	// the same angle or the shot flies where the player is pointing.
	int m_AimUntilTick = -1;
	int m_LastShotTick = -1;
	// No second shot before the one already on its way has arrived.
	int m_ShotLandsTick = -1;
	float m_LastSearchTime = 0.0f;

	void Reset();
	bool Predict(int LocalId, int StartTick);
	bool Search(int FireTick, vec2 FirePos);
	// Re-checks the shot that was found against the tick it would leave on now.
	// A plan is only good for the tick it was made for, and the search is too
	// expensive to redo every tick.
	bool Revalidate(int FireTick, vec2 FirePos);
	// True when the tee is frozen at that tick and no tile would freeze it again.
	bool IsUsefulTick(int Tick) const;
	// Walks the bounce path of a shot, the way CLaser::DoBounce does.
	void TraceLaser(vec2 Pos, vec2 Dir, float Energy, int FireTick, int MaxBounces, float BounceCost, int BounceTicks);
	// The bounce budget the shot is followed with, capped by what the server
	// allows and by the last tick that is still worth hitting.
	int BounceBudget(int FireTick, int BounceTicks, int TunedBounces) const;
	// True when a tile in the swept range would freeze the tee again.
	bool TouchesFreeze(vec2 PrevPos, vec2 Pos) const;
	bool IsFreezeIndex(int Index) const;
	const CFlightStep *FlightAt(int Tick) const;
	void RenderPlan() const;
	// A line of text under the crosshair, because without it the module is
	// invisible until it happens to fire.
	void RenderStatus() const;

	// What the module would say about right now.
	enum class EStatus
	{
		QUIET,
		READY,
		SEARCHING,
		NO_LASER,
		TOO_LATE,
	};
	EStatus m_Status = EStatus::QUIET;

	static void ConUnfreezeShoot(IConsole::IResult *pResult, void *pUserData);
};

#endif
