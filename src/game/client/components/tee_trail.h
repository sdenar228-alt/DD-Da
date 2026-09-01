#ifndef GAME_CLIENT_COMPONENTS_TEE_TRAIL_H
#define GAME_CLIENT_COMPONENTS_TEE_TRAIL_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>

#include <engine/shared/protocol.h>

#include <deque>

// A ribbon behind every tee, tracing where it has just been. Drawn under the
// players, so a tee always sits on top of its own trail.
class CTeeTrail : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnMapLoad() override { Clear(); }
	void OnReset() override { Clear(); }

private:
	// One remembered position. Sampled every frame rather than every tick, so
	// the ribbon stays smooth however far the frame rate is from the tick rate.
	class CPoint
	{
	public:
		vec2 m_Pos;
		float m_Time;
	};

	std::deque<CPoint> m_aTrails[MAX_CLIENTS];

	void Clear();
	void Sample(int ClientId, vec2 Pos, float Now, float MaxAge);
	void Draw(int ClientId, float Now, float MaxAge) const;
	// The colour of one tee's ribbon under the current mode.
	ColorRGBA TrailColor(int ClientId, float Now) const;
};

#endif
