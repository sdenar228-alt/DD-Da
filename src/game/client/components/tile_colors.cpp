#include "tile_colors.h"

#include <engine/shared/config.h>

#include <game/client/components/camera.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>

void CTileColors::RebuildBuckets()
{
	const struct
	{
		int m_Tile;
		unsigned m_Color;
	} aConfigured[] = {
		{TILE_SOLID, g_Config.m_ClCustomTileColorHookable},
		{TILE_NOHOOK, g_Config.m_ClCustomTileColorUnhookable},
		{TILE_DEATH, g_Config.m_ClCustomTileColorDeath},
		{TILE_FREEZE, g_Config.m_ClCustomTileColorFreeze},
		{TILE_UNFREEZE, g_Config.m_ClCustomTileColorUnfreeze},
		{TILE_DFREEZE, g_Config.m_ClCustomTileColorDeepFreeze},
		{TILE_DUNFREEZE, g_Config.m_ClCustomTileColorDeepUnfreeze},
		{TILE_LFREEZE, g_Config.m_ClCustomTileColorLiveFreeze},
		{TILE_LUNFREEZE, g_Config.m_ClCustomTileColorLiveUnfreeze},
		{TILE_NOLASER, g_Config.m_ClCustomTileColorNoLaser},
		{TILE_THROUGH, g_Config.m_ClCustomTileColorThrough},
		{TILE_THROUGH_CUT, g_Config.m_ClCustomTileColorThrough},
		{TILE_THROUGH_ALL, g_Config.m_ClCustomTileColorThrough},
		{TILE_THROUGH_DIR, g_Config.m_ClCustomTileColorThrough},
	};
	static_assert(std::size(aConfigured) == std::tuple_size_v<decltype(m_aLastColors)>);

	bool Changed = !m_BucketsValid;
	for(size_t i = 0; i < std::size(aConfigured); ++i)
	{
		if(m_aLastColors[i] != aConfigured[i].m_Color)
		{
			m_aLastColors[i] = aConfigured[i].m_Color;
			Changed = true;
		}
	}
	if(!Changed)
		return;

	m_vBuckets.clear();
	m_aBucketForTile.fill(-1);
	for(const auto &Configured : aConfigured)
	{
		const ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(Configured.m_Color, true));
		if(Color.a <= 0.0f)
			continue;
		m_aBucketForTile[Configured.m_Tile] = (int)m_vBuckets.size();
		m_vBuckets.push_back({Color, {}});
	}
	m_BucketsValid = true;
}

void CTileColors::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!g_Config.m_ClCustomTileColors)
		return;

	RebuildBuckets();
	if(m_vBuckets.empty())
		return;

	const CCollision *pCollision = Collision();
	const int MapWidth = pCollision->GetWidth();
	const int MapHeight = pCollision->GetHeight();
	if(MapWidth <= 0 || MapHeight <= 0)
		return;

	// Map the screen to the game world explicitly instead of relying on whatever
	// the map renderer happened to leave behind, and restore it afterwards.
	const CScreenRect SavedScreenRect = Graphics()->GetScreen();
	const CCamera *pCamera = &GameClient()->m_Camera;
	const CScreenRect ScreenRect = Graphics()->MapScreenToWorld(
		pCamera->m_Center.x, pCamera->m_Center.y, 100.0f, 100.0f, 100.0f, 0, 0,
		Graphics()->ScreenAspect(), pCamera->m_Zoom);
	Graphics()->MapScreen(ScreenRect);
	const int StartX = std::max(0, (int)std::floor(ScreenRect.m_TopLeft.x / 32.0f));
	const int StartY = std::max(0, (int)std::floor(ScreenRect.m_TopLeft.y / 32.0f));
	const int EndX = std::min(MapWidth - 1, (int)std::floor(ScreenRect.m_BottomRight.x / 32.0f));
	const int EndY = std::min(MapHeight - 1, (int)std::floor(ScreenRect.m_BottomRight.y / 32.0f));
	if(StartX > EndX || StartY > EndY)
	{
		Graphics()->MapScreen(SavedScreenRect);
		return;
	}

	for(SBucket &Bucket : m_vBuckets)
		Bucket.m_vQuads.clear();

	const bool UseFront = g_Config.m_ClCustomTileColorsFront != 0;
	for(int y = StartY; y <= EndY; ++y)
	{
		for(int x = StartX; x <= EndX; ++x)
		{
			const int MapIndex = y * MapWidth + x;

			int Bucket = m_aBucketForTile[pCollision->GetTileIndex(MapIndex) & 0xFF];
			if(Bucket < 0 && UseFront)
				Bucket = m_aBucketForTile[pCollision->GetFrontTileIndex(MapIndex) & 0xFF];
			if(Bucket < 0 || (size_t)Bucket >= m_vBuckets.size())
				continue;

			m_vBuckets[Bucket].m_vQuads.emplace_back(x * 32.0f, y * 32.0f, 32.0f, 32.0f);
		}
	}

	Graphics()->TextureClear();
	Graphics()->BlendNormal();
	// The vertex buffer behind QuadsDrawTL is a fixed size array, so the quads have
	// to be handed over in chunks. It is only flushed once another chunk of the
	// size that was just drawn would no longer fit, so the short trailing chunk of
	// a bucket can leave it nearly full; every bucket therefore ends its own batch
	// and the next one starts from an empty buffer.
	constexpr size_t MaxQuadsPerCall = 1024;
	for(const SBucket &Bucket : m_vBuckets)
	{
		if(Bucket.m_vQuads.empty())
			continue;
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Bucket.m_Color);
		for(size_t Offset = 0; Offset < Bucket.m_vQuads.size(); Offset += MaxQuadsPerCall)
		{
			const size_t Count = std::min(MaxQuadsPerCall, Bucket.m_vQuads.size() - Offset);
			Graphics()->QuadsDrawTL(Bucket.m_vQuads.data() + Offset, (int)Count);
		}
		Graphics()->QuadsEnd();
	}

	Graphics()->MapScreen(SavedScreenRect);
}
