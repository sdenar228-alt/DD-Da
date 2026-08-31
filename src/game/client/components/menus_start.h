/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MENUS_START_H
#define GAME_CLIENT_COMPONENTS_MENUS_START_H

#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

class CMenusStart : public CComponentInterfaces
{
public:
	void RenderStartMenu(CUIRect MainView);

private:
	bool CheckHotKey(int Key) const;

	// The logo above the buttons. Loaded the first time the menu is drawn rather
	// than at startup, because the start menu is the only thing that uses it and
	// a client that goes straight into a server never needs it at all.
	IGraphics::CTextureHandle m_LogoTexture;
	bool m_LogoLoaded = false;
};

#endif
