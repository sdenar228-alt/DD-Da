#include "music_island.h"

#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <vector>

void CMusicIsland::OnInit()
{
	m_ArtworkTexture.Invalidate();
}

// The binds control the player with the island turned off as well, and only the
// worker executes the queue, so it has to be running before anything is queued.
void CMusicIsland::SendCommand(CWindowsMusic::ECommand Command)
{
	m_Music.Start();
	m_Music.SendCommand(Command);
}

void CMusicIsland::ConMusicPlayPause(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CMusicIsland *>(pUserData)->SendCommand(CWindowsMusic::ECommand::PLAY_PAUSE);
}

void CMusicIsland::ConMusicNext(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CMusicIsland *>(pUserData)->SendCommand(CWindowsMusic::ECommand::NEXT);
}

void CMusicIsland::ConMusicPrev(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CMusicIsland *>(pUserData)->SendCommand(CWindowsMusic::ECommand::PREVIOUS);
}

void CMusicIsland::OnConsoleInit()
{
	Console()->Register("music_play_pause", "", CFGFLAG_CLIENT, ConMusicPlayPause, this, "Pause or resume the track shown in the music island");
	Console()->Register("music_next", "", CFGFLAG_CLIENT, ConMusicNext, this, "Skip to the next track");
	Console()->Register("music_prev", "", CFGFLAG_CLIENT, ConMusicPrev, this, "Go back to the previous track");
}

void CMusicIsland::OnShutdown()
{
	m_Music.Stop();
	if(m_ArtworkTexture.IsValid())
		Graphics()->UnloadTexture(&m_ArtworkTexture);
}

void CMusicIsland::OnRender()
{
	if(!Update())
	{
		// A drag cannot outlive the island, it would jump to wherever the cursor
		// happens to be when it comes back.
		m_Dragging = false;
		return;
	}
	const CUIRect *pScreen = Ui()->Screen();
	Render(pScreen->w, pScreen->h);
}

// Keeps the worker and the artwork in sync and reports whether there is
// anything to draw.
bool CMusicIsland::Update()
{
	if(!g_Config.m_ClMusicIsland)
	{
		m_Visible = 0.0f;
		return false;
	}

	const bool InGame = Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK;
	if(InGame ? !g_Config.m_ClMusicIslandIngame : !g_Config.m_ClMusicIslandMenu)
		return false;

	// The query runs on its own thread, starting it is cheap and idempotent.
	m_Music.Start();

	m_Track = m_Music.Track();

	std::vector<uint8_t> vArtwork;
	int ArtWidth = 0, ArtHeight = 0;
	if(m_Music.TakeArtwork(vArtwork, ArtWidth, ArtHeight))
	{
		// The old cover goes even when the new track has none, the placeholder
		// square is drawn for it instead.
		if(m_ArtworkTexture.IsValid())
			Graphics()->UnloadTexture(&m_ArtworkTexture);
		if(ArtWidth > 0 && ArtHeight > 0)
		{
			CImageInfo Image;
			Image.m_Width = ArtWidth;
			Image.m_Height = ArtHeight;
			Image.m_Format = CImageInfo::FORMAT_RGBA;
			Image.Allocate();
			mem_copy(Image.m_pData, vArtwork.data(), std::min(vArtwork.size(), Image.DataSize()));
			m_ArtworkTexture = Graphics()->LoadTextureRawMove(Image, 0, "music artwork");
			if(m_ArtworkTexture.IsNullTexture())
				m_ArtworkTexture.Invalidate();
		}
	}

	if(m_Track.m_Revision != m_ShownRevision)
	{
		m_ShownRevision = m_Track.m_Revision;
		// A fresh track shows the full layout for a moment.
		m_HighlightUntil = Client()->LocalTime() + 5.0f;
	}

	// Slide in while something is playing, slide out when it stops.
	const bool Wanted = !m_Track.m_Title.empty() && (m_Track.m_Playing || g_Config.m_ClMusicIslandWhenPaused);
	const float Speed = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f) * 6.0f;
	m_Visible += Wanted ? Speed : -Speed;
	m_Visible = std::clamp(m_Visible, 0.0f, 1.0f);
	return m_Visible > 0.001f;
}

// Icons are drawn from plain shapes, no texture needed.
enum
{
	ICON_PREV = 0,
	ICON_PLAY,
	ICON_PAUSE,
	ICON_NEXT,
};

namespace {
void DrawTriangle(IGraphics *pGraphics, float x, float y, float w, float h, bool PointRight)
{
	// A quad with two corners collapsed makes a triangle.
	IGraphics::CFreeformItem Item = PointRight ?
						IGraphics::CFreeformItem(x, y, x, y + h, x + w, y + h / 2.0f, x + w, y + h / 2.0f) :
						IGraphics::CFreeformItem(x + w, y, x + w, y + h, x, y + h / 2.0f, x, y + h / 2.0f);
	pGraphics->QuadsDrawFreeform(&Item, 1);
}
} // namespace

// Dragging the pill itself moves the island. The position is written back to
// the config in permille, the same units the settings sliders use.
void CMusicIsland::DoDrag(const CUIRect &Pill, const CUIRect &Buttons, float Width, float Height, float PillWidth, float PillHeight, float Margin)
{
	const float RoomX = Width - PillWidth;
	const float RoomY = std::max(0.0f, Height - PillHeight - 2.0f * Margin);

	if(m_Dragging)
	{
		if(!Ui()->MouseButton(0))
		{
			m_Dragging = false;
		}
		else
		{
			if(RoomX > 0.001f)
				g_Config.m_ClMusicIslandX = std::clamp(round_to_int((Ui()->MouseX() - m_DragOffsetX) / RoomX * 1000.0f), 0, 1000);
			if(RoomY > 0.001f)
				g_Config.m_ClMusicIslandY = std::clamp(round_to_int((Ui()->MouseY() - m_DragOffsetY - Margin) / RoomY * 1000.0f), 0, 1000);
		}
	}
	// The transport buttons keep their own area, grabbing anywhere else moves it.
	else if(Ui()->MouseButtonClicked(0) && Ui()->MouseHovered(&Pill) && !Ui()->MouseHovered(&Buttons))
	{
		m_Dragging = true;
		m_DragOffsetX = Ui()->MouseX() - Pill.x;
		m_DragOffsetY = Ui()->MouseY() - Pill.y;
	}

	// Claims the hover so a menu button below the island cannot be pressed
	// through it.
	if(Ui()->MouseHovered(&Pill))
		Ui()->SetHotItem(&m_DragId);
}

bool CMusicIsland::DoIslandButton(const CUIRect &Rect, int Icon, float Alpha, bool Clickable)
{
	bool Clicked = false;
	float Highlight = 0.0f;
	if(Clickable)
	{
		// The island draws after the menus have closed their check window, where
		// an activation is thrown away before the release arrives. So the press is
		// read straight from the mouse instead of going through the hot item.
		if(Ui()->MouseHovered(&Rect))
		{
			Highlight = 0.35f;
			Clicked = Ui()->MouseButtonClicked(0);
		}
	}

	const ColorRGBA Color(0.88f + Highlight * 0.12f, 0.90f + Highlight * 0.10f, 0.95f, Alpha);
	const float w = Rect.w;
	const float h = Rect.h;
	const float BarWidth = w * 0.18f;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	if(Icon == ICON_PLAY)
	{
		DrawTriangle(Graphics(), Rect.x + w * 0.22f, Rect.y, w * 0.62f, h, true);
	}
	else if(Icon == ICON_PAUSE)
	{
		const IGraphics::CQuadItem aBars[2] = {
			IGraphics::CQuadItem(Rect.x + w * 0.26f, Rect.y, BarWidth, h),
			IGraphics::CQuadItem(Rect.x + w * 0.56f, Rect.y, BarWidth, h),
		};
		Graphics()->QuadsDrawTL(aBars, 2);
	}
	else if(Icon == ICON_PREV)
	{
		const IGraphics::CQuadItem Bar(Rect.x + w * 0.16f, Rect.y, BarWidth, h);
		Graphics()->QuadsDrawTL(&Bar, 1);
		DrawTriangle(Graphics(), Rect.x + w * 0.34f, Rect.y, w * 0.5f, h, false);
	}
	else
	{
		DrawTriangle(Graphics(), Rect.x + w * 0.16f, Rect.y, w * 0.5f, h, true);
		const IGraphics::CQuadItem Bar(Rect.x + w * 0.66f, Rect.y, BarWidth, h);
		Graphics()->QuadsDrawTL(&Bar, 1);
	}
	Graphics()->QuadsEnd();

	return Clicked;
}

void CMusicIsland::Render(float Width, float Height)
{
	// Draw in a screen of its own and put the old one back, otherwise everything
	// after this would inherit the mapping.
	const CScreenRect SavedScreenRect = Graphics()->GetScreen();
	Ui()->MapScreen();

	const float Scale = g_Config.m_ClMusicIslandSize / 100.0f;
	const float PillHeight = 52.0f * Scale;
	const float Padding = 7.0f * Scale;
	const float ArtSize = PillHeight - 2.0f * Padding;
	const float TitleSize = 14.0f * Scale;
	const float ArtistSize = 11.6f * Scale;

	// Transport buttons on the right, like a phone shows them.
	const float ButtonSize = 12.0f * Scale;
	const float ButtonGap = 6.0f * Scale;
	const float ButtonsWidth = ButtonSize * 3.0f + ButtonGap * 2.0f;

	const float TitleWidth = TextRender()->TextWidth(TitleSize, m_Track.m_Title.c_str());
	const float ArtistWidth = TextRender()->TextWidth(ArtistSize, m_Track.m_Artist.c_str());
	float TextWidth = std::max(TitleWidth, ArtistWidth);
	TextWidth = std::clamp(TextWidth, 48.0f * Scale, 220.0f * Scale);

	const float PillWidth = Padding + ArtSize + Padding * 1.6f + TextWidth + Padding * 1.6f + ButtonsWidth + Padding;

	// Free placement, the config holds the position in permille of the room the
	// island can move in, so it stays put at any resolution.
	const float Margin = 8.0f * Scale;
	const float BaseX = (Width - PillWidth) * (g_Config.m_ClMusicIslandX / 1000.0f);
	const float BaseY = Margin + std::max(0.0f, Height - PillHeight - 2.0f * Margin) * (g_Config.m_ClMusicIslandY / 1000.0f);

	const float Ease = 1.0f - std::pow(1.0f - m_Visible, 3.0f);

	CUIRect Pill;
	Pill.x = BaseX;
	Pill.y = BaseY - (1.0f - Ease) * PillHeight * 0.6f;
	Pill.w = PillWidth;
	Pill.h = PillHeight;

	const float Alpha = m_Visible * g_Config.m_ClMusicIslandOpacity / 100.0f;
	Pill.Draw(ColorRGBA(0.04f, 0.05f, 0.07f, 0.94f * Alpha), IGraphics::CORNER_ALL, PillHeight / 2.0f);

	// Artwork on the left, a neutral square when the player gives none.
	CUIRect Art, Rest;
	Pill.VSplitLeft(Padding, nullptr, &Rest);
	Rest.VSplitLeft(ArtSize, &Art, &Rest);
	Art.y += Padding;
	Art.h = ArtSize;
	if(m_ArtworkTexture.IsValid())
	{
		Graphics()->TextureSet(m_ArtworkTexture);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		const IGraphics::CQuadItem QuadItem(Art.x, Art.y, Art.w, Art.h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
	else
	{
		Art.Draw(ColorRGBA(0.22f, 0.23f, 0.28f, Alpha), IGraphics::CORNER_ALL, ArtSize / 5.0f);
	}

	// Title above, artist below, both always visible.
	Rest.VSplitLeft(Padding * 1.6f, nullptr, &Rest);
	CUIRect TextArea;
	Rest.VSplitLeft(TextWidth, &TextArea, &Rest);
	TextArea.y += Padding * 0.8f;
	TextArea.h = PillHeight - Padding * 2.6f;

	SLabelProperties Props;
	Props.m_MaxWidth = TextArea.w;
	Props.m_EllipsisAtEnd = true;

	CUIRect TitleRect, ArtistRect;
	TextArea.HSplitTop(TextArea.h * 0.55f, &TitleRect, &ArtistRect);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	Ui()->DoLabel(&TitleRect, m_Track.m_Title.c_str(), TitleSize, TEXTALIGN_ML, Props);
	if(!m_Track.m_Artist.empty())
	{
		TextRender()->TextColor(0.66f, 0.68f, 0.75f, Alpha);
		Ui()->DoLabel(&ArtistRect, m_Track.m_Artist.c_str(), ArtistSize, TEXTALIGN_ML, Props);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	// Clicking only makes sense where the mouse is a cursor; in game it aims, so
	// the console commands are the way there.
	const bool Clickable = GameClient()->m_Menus.IsActive() || Client()->State() == IClient::STATE_OFFLINE;

	Rest.VSplitLeft(Padding * 1.6f, nullptr, &Rest);
	const CUIRect Buttons = Rest;
	CUIRect Button;
	Rest.y = Pill.y + (PillHeight - ButtonSize) / 2.0f;
	Rest.h = ButtonSize;

	Rest.VSplitLeft(ButtonSize, &Button, &Rest);
	if(DoIslandButton(Button, ICON_PREV, Alpha, Clickable))
		m_Music.SendCommand(CWindowsMusic::ECommand::PREVIOUS);
	Rest.VSplitLeft(ButtonGap, nullptr, &Rest);
	Rest.VSplitLeft(ButtonSize, &Button, &Rest);
	if(DoIslandButton(Button, m_Track.m_Playing ? ICON_PAUSE : ICON_PLAY, Alpha, Clickable))
		m_Music.SendCommand(CWindowsMusic::ECommand::PLAY_PAUSE);
	Rest.VSplitLeft(ButtonGap, nullptr, &Rest);
	Rest.VSplitLeft(ButtonSize, &Button, &Rest);
	if(DoIslandButton(Button, ICON_NEXT, Alpha, Clickable))
		m_Music.SendCommand(CWindowsMusic::ECommand::NEXT);

	if(Clickable)
		DoDrag(Pill, Buttons, Width, Height, PillWidth, PillHeight, Margin);
	else
		m_Dragging = false;

	// Progress along the bottom edge.
	if(m_Track.m_Duration > 0.0)
	{
		const float BarHeight = 3.0f * Scale;
		const float Inset = PillHeight * 0.28f;
		CUIRect Line;
		Line.x = Pill.x + Inset;
		Line.w = Pill.w - Inset * 2.0f;
		Line.h = BarHeight;
		Line.y = Pill.y + Pill.h - BarHeight - 4.0f * Scale;
		Line.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.20f * Alpha), IGraphics::CORNER_ALL, BarHeight / 2.0f);

		CUIRect Filled = Line;
		Filled.w = Line.w * (float)std::clamp(m_Track.m_Position / m_Track.m_Duration, 0.0, 1.0);
		if(Filled.w > BarHeight)
			Filled.Draw(ColorRGBA(0.42f, 0.68f, 1.0f, 0.95f * Alpha), IGraphics::CORNER_ALL, BarHeight / 2.0f);
	}

	// The menus draw the cursor before this, so the island would swallow it.
	// Drawn again, and only over the island, so nothing is doubled elsewhere.
	if(GameClient()->m_Menus.IsActive() && (m_Dragging || Ui()->MouseHovered(&Pill)))
		RenderTools()->RenderCursor(Ui()->MousePos(), 24.0f);

	Graphics()->MapScreen(SavedScreenRect);
}
