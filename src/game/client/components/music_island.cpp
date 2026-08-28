#include "music_island.h"

#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <vector>

void CMusicIsland::OnInit()
{
	m_ArtworkTexture.Invalidate();
}

void CMusicIsland::ConMusicPlayPause(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CMusicIsland *>(pUserData)->m_Music.SendCommand(CWindowsMusic::ECommand::PLAY_PAUSE);
}

void CMusicIsland::ConMusicNext(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CMusicIsland *>(pUserData)->m_Music.SendCommand(CWindowsMusic::ECommand::NEXT);
}

void CMusicIsland::ConMusicPrev(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CMusicIsland *>(pUserData)->m_Music.SendCommand(CWindowsMusic::ECommand::PREVIOUS);
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
	m_Started = false;
	if(m_ArtworkTexture.IsValid())
		Graphics()->UnloadTexture(&m_ArtworkTexture);
}

void CMusicIsland::OnRender()
{
	if(!g_Config.m_ClMusicIsland)
	{
		if(m_Started)
		{
			m_Music.Stop();
			m_Started = false;
			m_Visible = 0.0f;
		}
		return;
	}

	const bool InGame = Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK;
	if(InGame ? !g_Config.m_ClMusicIslandIngame : !g_Config.m_ClMusicIslandMenu)
		return;

	// The query runs on its own thread, starting it is cheap and idempotent.
	if(!m_Started)
	{
		m_Music.Start();
		m_Started = true;
	}

	m_Track = m_Music.Track();

	std::vector<uint8_t> vArtwork;
	int ArtWidth = 0, ArtHeight = 0;
	if(m_Music.TakeArtwork(vArtwork, ArtWidth, ArtHeight) && ArtWidth > 0 && ArtHeight > 0)
	{
		if(m_ArtworkTexture.IsValid())
			Graphics()->UnloadTexture(&m_ArtworkTexture);
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
	if(m_Visible <= 0.001f)
		return;

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Render(Width, Height);
}

void CMusicIsland::Render(float Width, float Height)
{
	// Draw in a screen of its own and put the old one back, otherwise everything
	// after this would inherit the mapping.
	const CScreenRect SavedScreenRect = Graphics()->GetScreen();
	Graphics()->MapScreenToSize(Width, Height);

	const float Scale = g_Config.m_ClMusicIslandSize / 100.0f;
	const float PillHeight = 22.0f * Scale;
	const float Padding = 4.0f * Scale;
	const float ArtSize = PillHeight - 2.0f * Padding;

	const bool Highlighted = Client()->LocalTime() < m_HighlightUntil;
	const float TitleSize = 8.0f * Scale;
	const float ArtistSize = 6.5f * Scale;

	// The three bars on the right get their own column.
	const float BarWidth = 1.2f * Scale;
	const float BarGap = 1.0f * Scale;
	const float EqualiserWidth = BarWidth * 3.0f + BarGap * 2.0f;

	// Width follows the text, so a short title gives a short pill.
	const float TitleWidth = TextRender()->TextWidth(TitleSize, m_Track.m_Title.c_str());
	const float ArtistWidth = TextRender()->TextWidth(ArtistSize, m_Track.m_Artist.c_str());
	float TextWidth = std::max(TitleWidth, Highlighted ? ArtistWidth : 0.0f);
	TextWidth = std::clamp(TextWidth, 20.0f * Scale, 150.0f * Scale);

	const float PillWidth = Padding + ArtSize + Padding + TextWidth + Padding + EqualiserWidth + Padding;

	// Free placement, the config holds the position in permille of the room the
	// island can move in, so it stays put at any resolution.
	const float Margin = 4.0f * Scale;
	const float BaseX = (Width - PillWidth) * (g_Config.m_ClMusicIslandX / 1000.0f);
	const float BaseY = Margin + std::max(0.0f, Height - PillHeight - 2.0f * Margin) * (g_Config.m_ClMusicIslandY / 1000.0f);

	// Eases in with a small drop, so it works the same wherever it sits.
	const float Ease = 1.0f - std::pow(1.0f - m_Visible, 3.0f);

	CUIRect Pill;
	Pill.x = BaseX;
	Pill.y = BaseY - (1.0f - Ease) * PillHeight * 0.6f;
	Pill.w = PillWidth;
	Pill.h = PillHeight;

	const float Alpha = m_Visible * g_Config.m_ClMusicIslandOpacity / 100.0f;
	Pill.Draw(ColorRGBA(0.03f, 0.03f, 0.04f, 0.92f * Alpha), IGraphics::CORNER_ALL, PillHeight / 2.0f);

	// Artwork, or a neutral square when the player gives none.
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
		Art.Draw(ColorRGBA(0.25f, 0.25f, 0.3f, Alpha), IGraphics::CORNER_ALL, ArtSize / 4.0f);
	}

	Rest.VSplitLeft(Padding, nullptr, &Rest);
	Rest.VSplitRight(EqualiserWidth + Padding * 2.0f, &Rest, nullptr);

	// Title on top, artist below it while the track is fresh.
	CUIRect TitleRect = Rest;
	SLabelProperties Props;
	Props.m_MaxWidth = Rest.w;
	Props.m_EllipsisAtEnd = true;

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	if(Highlighted && !m_Track.m_Artist.empty())
	{
		CUIRect ArtistRect;
		TitleRect.HSplitTop(Rest.h * 0.55f, &TitleRect, &ArtistRect);
		Ui()->DoLabel(&TitleRect, m_Track.m_Title.c_str(), TitleSize, TEXTALIGN_ML, Props);
		TextRender()->TextColor(0.72f, 0.72f, 0.78f, Alpha);
		Ui()->DoLabel(&ArtistRect, m_Track.m_Artist.c_str(), ArtistSize, TEXTALIGN_ML, Props);
	}
	else
	{
		Ui()->DoLabel(&TitleRect, m_Track.m_Title.c_str(), TitleSize, TEXTALIGN_ML, Props);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	// Progress bar along the bottom of the pill, only when the player reports a
	// length. It doubles as the "how much is left" indicator.
	if(m_Track.m_Duration > 0.0)
	{
		const float BarHeight = 1.4f * Scale;
		const float Inset = PillHeight / 2.0f * 0.6f;
		CUIRect Line;
		Line.x = Pill.x + Inset;
		Line.w = Pill.w - Inset * 2.0f;
		Line.h = BarHeight;
		Line.y = Pill.y + Pill.h - BarHeight - 1.5f * Scale;
		Line.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.18f * Alpha), IGraphics::CORNER_ALL, BarHeight / 2.0f);

		CUIRect Filled = Line;
		Filled.w = Line.w * (float)std::clamp(m_Track.m_Position / m_Track.m_Duration, 0.0, 1.0);
		if(Filled.w > BarHeight)
			Filled.Draw(ColorRGBA(0.55f, 0.78f, 1.0f, 0.85f * Alpha), IGraphics::CORNER_ALL, BarHeight / 2.0f);
	}

	// Three little bars that bounce while the track plays.
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	const float BarMaxHeight = ArtSize * 0.7f;
	const float BarBottom = Pill.y + Pill.h - Padding;
	const float BarX = Pill.x + Pill.w - Padding - EqualiserWidth;
	for(int i = 0; i < 3; ++i)
	{
		float Factor = 0.35f;
		if(m_Track.m_Playing)
		{
			// Different speeds per bar so it does not look mechanical.
			const float Phase = Client()->LocalTime() * (5.0f + i * 1.7f) + i * 2.0f;
			Factor = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(Phase));
		}
		const float BarHeight = BarMaxHeight * Factor;
		Graphics()->SetColor(0.55f, 0.78f, 1.0f, Alpha);
		const IGraphics::CQuadItem QuadItem(BarX + i * (BarWidth + BarGap), BarBottom - BarHeight, BarWidth, BarHeight);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	}
	Graphics()->QuadsEnd();

	Graphics()->MapScreen(SavedScreenRect);
}
