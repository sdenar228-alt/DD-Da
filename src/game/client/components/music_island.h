#ifndef GAME_CLIENT_COMPONENTS_MUSIC_ISLAND_H
#define GAME_CLIENT_COMPONENTS_MUSIC_ISLAND_H

#include <engine/console.h>
#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include "custom_music_win.h"

#include <string>

// A rounded pill at the top of the screen showing what is playing right now,
// the way a phone shows it. The track comes from the Windows media session, so
// it works with any player that reports to the system.
class CMusicIsland : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnConsoleInit() override;
	void OnShutdown() override;
	void OnRender() override;

private:
	CWindowsMusic m_Music;
	bool m_Started = false;
	bool Update();

	IGraphics::CTextureHandle m_ArtworkTexture;

	CWindowsMusic::CTrack m_Track;
	uint64_t m_ShownRevision = 0;
	// 0 hidden, 1 fully out. Eased so it slides in and out.
	float m_Visible = 0.0f;
	// Counts down after a track change, the island is wider while it runs.
	float m_HighlightUntil = 0.0f;

	void Render(float Width, float Height);
	// Moves the island when it is dragged around.
	void DoDrag(const CUIRect &Pill, const CUIRect &Buttons, float Width, float Height, float PillWidth, float PillHeight, float Margin);
	int m_DragId = 0;
	bool m_Dragging = false;
	float m_DragOffsetX = 0.0f;
	float m_DragOffsetY = 0.0f;

	// Draws one of the transport buttons and reports a click on it.
	bool DoIslandButton(const CUIRect &Rect, int Icon, float Alpha, bool Clickable);

	static void ConMusicPlayPause(IConsole::IResult *pResult, void *pUserData);
	static void ConMusicNext(IConsole::IResult *pResult, void *pUserData);
	static void ConMusicPrev(IConsole::IResult *pResult, void *pUserData);
};

#endif
