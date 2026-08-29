#ifndef GAME_CLIENT_COMPONENTS_UNFREEZE_H
#define GAME_CLIENT_COMPONENTS_UNFREEZE_H

#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>
#include <game/client/prediction/gameworld.h>

#include <generated/protocol.h>

#include <array>
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

	// Both are called from CControls::SnapInput. The weapon request and the shot
	// go on the stored input, so the client's own bookkeeping stays in step; the
	// aim may only go on the send copy, never on the stored input.
	bool ApplyInput(CNetObj_PlayerInput *pInput);
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
	// No second shot before the one already on its way has arrived.
	int m_ShotLandsTick = -1;
	float m_LastSearchTime = 0.0f;

	// The weapon the player had before the module asked for the laser, plus one,
	// which is how the protocol carries it. -1 while the module is not holding
	// the field.
	int m_RestoreWeapon = -1;
	// How many more sends the laser request rides on. The server acts on the
	// wanted weapon out of two different copies of the input, so one send is not
	// reliably enough.
	int m_SwitchSends = 0;
	int m_SwitchDeadlineTick = -1;

	void Reset();
	bool Predict(int LocalId, int StartTick, int Horizon);
	// Sweeps the angles for one fire tick and keeps the best plan found so far.
	bool Search(int FireTick, vec2 FirePos, CCandidate &Best, float &BestScore, std::vector<CSegment> &vBestPath);
	// Which fire delays could put a bounce on a tick worth hitting. Bounces land
	// every BounceTicks, so firing a tick later moves all of them by a tick, and
	// only a couple of delays are ever worth tracing.
	int UsefulDelays(int PredTick, int BounceTicks, bool *pDelays, int MaxDelays) const;
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
