#ifndef GAME_CLIENT_COMPONENTS_TILE_COLORS_H
#define GAME_CLIENT_COMPONENTS_TILE_COLORS_H

#include <base/color.h>

#include <engine/graphics.h>

#include <game/client/component.h>

#include <array>
#include <vector>

// Draws a configurable colored overlay over the game layer, so that freeze,
// kill, hookable and similar tiles can be told apart at a glance without
// turning on the entities overlay.
class CTileColors : public CComponent
{
public:
	CTileColors() { m_aBucketForTile.fill(-1); }
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	// A new map invalidates the cached geometry, the old tiles are gone.
	void OnMapLoad() override { m_QuadsValid = false; }

private:
	// One bucket per colored tile type, filled with the quads to draw.
	struct SBucket
	{
		ColorRGBA m_Color;
		std::vector<IGraphics::CQuadItem> m_vQuads;
	};

	std::vector<SBucket> m_vBuckets;
	// Index into `m_vBuckets` per tile index, -1 for tiles that are not colored.
	std::array<int, 256> m_aBucketForTile;
	// The config values the current buckets were built from.
	std::array<unsigned, 14> m_aLastColors = {};
	bool m_BucketsValid = false;

	// The quads only change when the visible tiles do, which is when the camera
	// crosses a tile boundary, not every frame.
	int m_CachedStartX = 0;
	int m_CachedStartY = 0;
	int m_CachedEndX = -1;
	int m_CachedEndY = -1;
	bool m_CachedUseFront = false;
	bool m_QuadsValid = false;

	void RebuildBuckets();
	void RebuildQuads(int StartX, int StartY, int EndX, int EndY, bool UseFront);
};

#endif
