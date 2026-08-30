#ifndef GAME_CLIENT_COMPONENTS_UNFREEZE_H
#define GAME_CLIENT_COMPONENTS_UNFREEZE_H

#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>
#include <game/client/prediction/gameworld.h>

#include <generated/protocol.h>

#include <array>
#include <utility>
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

	// All three are called from CControls::SnapInput.
	//
	// Only the shot itself goes on the stored input, because the fire counter is
	// cumulative and the client's own bookkeeping has to stay in step with it.
	// The aim and the weapon request may only go on the send copy: both fields
	// belong to the player, and the weapon one in particular is sticky, so a
	// value left in the stored input disables their weapon wheel for good.
	bool ApplyInput(CNetObj_PlayerInput *pInput);
	bool ApplyWeapon(CNetObj_PlayerInput *pSendData);
	bool ApplyAim(CNetObj_PlayerInput *pInput) const;

private:
	// How many other tees are followed through the flight. They matter because a
	// laser stops at the first tee it touches, whoever that is.
	static constexpr int MAX_TRACKED = 4;

	// One predicted tick of the tee's flight.
	class CFlightStep
	{
	public:
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_PrevPos = vec2(0.0f, 0.0f);
		// Ticks of freeze left on this tick, zero when free.
		int m_FreezeTime = 0;
		// Neither of these can be lifted by a laser.
		bool m_Deep = false;
		bool m_Live = false;
		// True while a tile would freeze the tee again on this tick.
		bool m_OnFreeze = false;
		// Positions of the nearby tees on this tick, for the absorption test.
		std::array<vec2, MAX_TRACKED> m_aOthers = {};
		int m_NumOthers = 0;
	};

	// One stretch of the laser's flight, the way CLaser casts it.
	class CSegment
	{
	public:
		vec2 m_From = vec2(0.0f, 0.0f);
		vec2 m_To = vec2(0.0f, 0.0f);
		// The tick the game evaluates this stretch on, which is also the tick it
		// tests the players on.
		int m_EvalTick = 0;
		// How many walls the shot has hit before this stretch. It cannot touch
		// its owner while this is zero.
		int m_Bounces = 0;
	};

	// What one aim angle would achieve.
	class CCandidate
	{
	public:
		bool m_Hits = false;
		// The tick the shot reaches the tee on.
		int m_EvalTick = 0;
		// Ticks of freeze the hit actually takes off the tee. This is the value
		// of the shot; everything else is a tiebreak.
		int m_Saved = 0;
		// How many of the three ticks around the aimed one would also land.
		int m_Margin = 0;
		// How far the beam passes from the tee on the aimed tick.
		float m_Miss = 0.0f;
		// The nearest the beam came to a tick worth hitting, whether or not it
		// got there. The sweep ranks angles by this, so a window narrower than
		// the coarse step is still found.
		float m_Approach = 0.0f;
		// True when the freed stretch ends because a tile freezes the tee again
		// rather than because the freeze would have run out anyway. Those windows
		// are short but they are the whole prize: they hand back control.
		bool m_TileEnd = false;
		// The tick the shot has to leave on for this plan to hold.
		int m_FireTick = 0;
		int m_TargetX = 0;
		int m_TargetY = 0;
		vec2 m_HitPos = vec2(0.0f, 0.0f);
	};

	// The world the flight is simulated in, kept as a member because copying one
	// allocates every entity in it.
	CGameWorld m_ScratchWorld;

	std::vector<CFlightStep> m_vFlight;
	// The tick m_vFlight[0] belongs to.
	int m_FirstFlightTick = -1;
	int m_FirstUsefulTick = -1;
	int m_LastUsefulTick = -1;
	bool m_FreezeAhead = false;
	// A box around every tick worth hitting. A beam that cannot reach it with the
	// energy it has left is abandoned instead of traced to the end.
	bool m_HasUsefulBox = false;
	vec2 m_UsefulMin = vec2(0.0f, 0.0f);
	vec2 m_UsefulMax = vec2(0.0f, 0.0f);

	std::vector<CSegment> m_vSegments;
	std::vector<CSegment> m_vSolution;
	// Angle and how promising it looked, filled by the coarse sweep and read by
	// the refining pass. A member so the sweep does not allocate every frame.
	std::vector<std::pair<float, float>> m_vProbes;

	bool m_HasSolution = false;
	CCandidate m_Solution;
	// The plan is made for one tick. Firing on any other one aims at where the
	// tee was going to be at a different moment.
	int m_PlanFireTick = -1;
	vec2 m_SolutionDir = vec2(1.0f, 0.0f);

	// Set by the console command or by the automatic mode, consumed by the next
	// input that is actually sent.
	bool m_WantShot = false;
	// The aim that was fired, held for a few sends: a lost input message makes the
	// server count the press when the next one arrives, and that one has to carry
	// the same angle or the shot flies where the player is pointing.
	int m_FiredTargetX = 0;
	int m_FiredTargetY = 0;
	int m_AimUntilTick = -1;
	int m_LastShotTick = -1;
	// What the last search cost, shown in the status line. The sweep is the one
	// part of this module that can be felt in the frame rate.
	float m_LastSearchMs = 0.0f;
	// No second shot before the one already on its way has arrived.
	int m_ShotLandsTick = -1;
	float m_LastSearchTime = 0.0f;

	// The weapon the player was holding before the module asked for the laser,
	// as a plain weapon index. -1 while the module is not touching the field.
	// It has to be the weapon itself rather than whatever the input carried:
	// that field is zero whenever the player last used the wheel, and writing
	// the zero back does not restore anything, it just leaves them on the laser.
	int m_RestoreWeapon = -1;
	// True while the laser is being asked for. The server only acts on the
	// wanted weapon when two inputs in a row carry it, because the tick it takes
	// the value from is one behind the tick it takes the request from, so the
	// field is written on every send until the switch has happened.
	bool m_WantLaser = false;
	int m_SwitchDeadlineTick = -1;

	void Reset();
	bool Predict(int LocalId, int StartTick, int Horizon);
	// Sweeps the angles for one fire tick and keeps the best plan found so far.
	// Gives up at the deadline with whatever it has, so a heavy setting costs
	// frames it was allowed to cost rather than however many it takes.
	bool Search(int FireTick, vec2 FirePos, CCandidate &Best, float &BestScore, std::vector<CSegment> &vBestPath, int64_t Deadline);
	// Which fire delays could put a bounce on a tick worth hitting. Bounces land
	// every BounceTicks, so firing a tick later moves all of them by a tick, and
	// only a couple of delays are ever worth tracing. Delays below MinDelay are
	// left out because the shot cannot leave that soon.
	int UsefulDelays(int PredTick, int BounceTicks, int MinDelay, int MaxDelay, bool *pDelays, int MaxDelays) const;
	// What a plan is worth, in ticks of freeze taken off plus how likely it is to
	// survive the flight being slightly off what was predicted.
	static float ScoreOf(const CCandidate &Candidate, int FireTick, float Radius);
	// Everything one aim angle would do, absorption included.
	CCandidate Evaluate(float Angle, int FireTick, vec2 FirePos, int MaxBounces, int BounceTicks, float Energy, float BounceCost);
	// Walks the bounce path of a shot, the way CLaser::DoBounce does.
	void TraceLaser(vec2 Pos, vec2 Dir, float Energy, int FireTick, int MaxBounces, float BounceCost, int BounceTicks);
	// Ticks of freeze a hit on that tick would take off the tee.
	int FreezeSaved(int Tick, bool *pTileEnd) const;
	// True when the tee is frozen there, the freeze is one a laser can lift, and
	// no tile puts it straight back.
	bool IsUsefulTick(int Tick) const;
	// True when a tile in the swept range would freeze the tee again.
	bool TouchesFreeze(vec2 PrevPos, vec2 Pos) const;
	bool IsFreezeIndex(int Index) const;
	const CFlightStep *FlightAt(int Tick) const;
	// The bounce budget, capped by what the server allows and by the last tick
	// that is still worth hitting.
	int BounceBudget(int FireTick, int BounceTicks, int TunedBounces) const;
	// The tuning the shot bounces by, and the one that decides how far it gets.
	// The game reads them from two different tune zones.
	const CTuningParams *LaserTuning(vec2 FirePos) const;
	const CTuningParams *ReachTuning(vec2 FirePos) const;
	// False on servers where a laser can never touch the tee that fired it.
	bool SelfHitPossible() const;
	// Says what the module just decided, into unfreeze.log in the config folder,
	// while cl_unfreeze_debug is on. A module that refuses silently is a module
	// nobody can tell apart from a broken one.
	void Debug(const char *pFormat, ...) const;
	mutable char m_aLastDebug[256] = {};
	mutable int m_LastDebugTick = -1;

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
		IMPOSSIBLE,
	};
	EStatus m_Status = EStatus::QUIET;

	static void ConUnfreezeShoot(IConsole::IResult *pResult, void *pUserData);
};

#endif
