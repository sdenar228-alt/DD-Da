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
	void OnInit() override;
	void OnShutdown() override;
	void OnRender() override;
	// A new map invalidates the cached geometry, the old tiles are gone.
	void OnMapLoad() override { m_QuadsValid = false; }
	// Disconnecting unloads the collision the quads were built from.
	void OnReset() override;

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
	// Hands the quads to the graphics card once, with the color of their bucket
	// baked into the vertices. Drawing them then costs neither vertex data nor
	// color state, which is the whole point: the geometry only changes when the
	// camera crosses a tile boundary, but it used to be streamed every frame.
	void UploadQuads();

	int m_QuadContainerIndex = -1;
	// How many quads the container holds. Zero means the view did not fit into
	// one and is streamed the old way instead.
	int m_ContainerQuadNum = 0;
};

#endif
