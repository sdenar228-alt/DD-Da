/* Settings page for the client specific features. */
#include "menus.h"

#include <base/str.h>

#include <engine/font_icons.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/components/hud.h>
#include <game/client/components/key_binder.h>
#include <game/client/components/skins.h>
#include <game/client/components/tooltips.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <algorithm>
#include <iterator>

enum
{
	LEVIATHAN_TAB_TEES = 0,
	LEVIATHAN_TAB_HOOK,
	LEVIATHAN_TAB_CROSSHAIR,
	LEVIATHAN_TAB_TILES,
	LEVIATHAN_TAB_BACKGROUND,
	LEVIATHAN_TAB_SOUNDS,
	LEVIATHAN_TAB_MODELS,
	LEVIATHAN_TAB_UNFREEZE,
	LEVIATHAN_TAB_TRAIL,
	LEVIATHAN_TAB_FRIENDS,
	LEVIATHAN_TAB_MISC,
	NUMBER_OF_LEVIATHAN_TABS,
};

// Layout constants, kept in sync with the appearance settings page so that both
// pages look the same.
static constexpr float LINE_SIZE = 20.0f;
static constexpr float COLOR_PICKER_LINE_SIZE = 25.0f;
static constexpr float COLOR_PICKER_LABEL_SIZE = 13.0f;
static constexpr float COLOR_PICKER_LINE_SPACING = 5.0f;
static constexpr float HEADLINE_FONT_SIZE = 20.0f;
static constexpr float HEADLINE_HEIGHT = 30.0f;
static constexpr float MARGIN_SMALL = 5.0f;
static constexpr float MARGIN_BETWEEN_VIEWS = 20.0f;

static ColorRGBA DefaultColor(unsigned Packed)
{
	return color_cast<ColorRGBA>(ColorHSLA(Packed, true));
}

void CMenus::DoLeviathanColorLine(CButtonContainer *pResetId, const void *pOpacityId, CUIRect *pView, const char *pLabel, unsigned *pColor, unsigned Default)
{
	DoLine_ColorPicker(pResetId, COLOR_PICKER_LINE_SIZE, COLOR_PICKER_LABEL_SIZE, 0.0f, pView,
		pLabel, pColor, DefaultColor(Default), false, nullptr, true);

	// The alpha byte of the packed HSLA value doubles as the opacity. Both
	// conversions round, otherwise the round trip loses a percent per write.
	const int OldOpacity = (int)((((*pColor) >> 24) & 0xFFu) * 100 + 127) / 255;
	int Opacity = OldOpacity;
	CUIRect Button;
	pView->HSplitTop(LINE_SIZE, &Button, pView);
	Button.VSplitLeft(10.0f, nullptr, &Button);
	Ui()->DoScrollbarOption(pOpacityId, &Opacity, &Button, Localize("Opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");
	if(Opacity != OldOpacity)
	{
		const unsigned Alpha = (unsigned)std::clamp((Opacity * 255 + 50) / 100, 0, 255);
		*pColor = ((*pColor) & 0x00FFFFFFu) | (Alpha << 24);
	}
	pView->HSplitTop(COLOR_PICKER_LINE_SPACING, nullptr, pView);
}

void CMenus::RenderSettingsLeviathan(CUIRect MainView)
{
	static int s_CurTab = LEVIATHAN_TAB_TEES;

	CUIRect TabBar, Button;
	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / (float)NUMBER_OF_LEVIATHAN_TABS;
	static CButtonContainer s_aPageTabs[NUMBER_OF_LEVIATHAN_TABS] = {};
	const char *apTabNames[NUMBER_OF_LEVIATHAN_TABS] = {
		Localize("Tees"),
		Localize("Hook"),
		Localize("Crosshair"),
		Localize("Tiles"),
		Localize("Background"),
		Localize("Sounds"),
		Localize("Models"),
		Localize("Unfreeze"),
		Localize("Trail"),
		Localize("Friends"),
		Localize("Misc")};

	for(int Tab = 0; Tab < NUMBER_OF_LEVIATHAN_TABS; ++Tab)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == 0 ? IGraphics::CORNER_L : (Tab == NUMBER_OF_LEVIATHAN_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			s_CurTab = Tab;
		}
	}

	MainView.HSplitTop(10.0f, nullptr, &MainView);

	// Files can appear while the game runs, so every page rescans its folder when
	// it is opened. The lists used to be built once, which made a freshly added
	// file impossible to pick.
	static int s_LastTab = -1;
	if(s_CurTab != s_LastTab)
	{
		s_LastTab = s_CurTab;
		m_BackgroundListLoaded = false;
		m_CrosshairListLoaded = false;
		m_AvatarListLoaded = false;
		m_SoundPackListLoaded = false;
		m_GameAssetListLoaded = false;
	}

	switch(s_CurTab)
	{
	case LEVIATHAN_TAB_TEES: RenderSettingsLeviathanTees(MainView); break;
	case LEVIATHAN_TAB_HOOK: RenderSettingsLeviathanHook(MainView); break;
	case LEVIATHAN_TAB_CROSSHAIR: RenderSettingsLeviathanCrosshair(MainView); break;
	case LEVIATHAN_TAB_TILES: RenderSettingsLeviathanTiles(MainView); break;
	case LEVIATHAN_TAB_BACKGROUND: RenderSettingsLeviathanBackground(MainView); break;
	case LEVIATHAN_TAB_SOUNDS: RenderSettingsLeviathanSounds(MainView); break;
	case LEVIATHAN_TAB_MODELS: RenderSettingsLeviathanModels(MainView); break;
	case LEVIATHAN_TAB_UNFREEZE: RenderSettingsLeviathanUnfreeze(MainView); break;
	case LEVIATHAN_TAB_TRAIL: RenderSettingsLeviathanTrail(MainView); break;
	case LEVIATHAN_TAB_FRIENDS: RenderSettingsLeviathanFriends(MainView); break;
	case LEVIATHAN_TAB_MISC: RenderSettingsLeviathanMisc(MainView); break;
	default: break;
	}
}

void CMenus::RenderSettingsLeviathanTees(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Tee outline"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomOutline, Localize("Draw tees with a custom outline"), &g_Config.m_ClCustomOutline, &LeftView, LINE_SIZE);

	if(g_Config.m_ClCustomOutline)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomOutlineOwn, Localize("Outline your own tee"), &g_Config.m_ClCustomOutlineOwn, &LeftView, LINE_SIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomOutlineOthers, Localize("Outline other tees"), &g_Config.m_ClCustomOutlineOthers, &LeftView, LINE_SIZE);

		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClCustomOutlineSize, &g_Config.m_ClCustomOutlineSize, &Button, Localize("Thickness"), 100, 200, &CUi::ms_LinearScrollbarScale, 0u, "%");

		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		static CButtonContainer s_OutlineColor;
		static int s_OutlineOpacity;
		DoLeviathanColorLine(&s_OutlineColor, &s_OutlineOpacity, &LeftView, Localize("Outline color"),
			&g_Config.m_ClCustomOutlineColor, DefaultConfig::ClCustomOutlineColor);
	}

	// ***** Shader ***** //
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	Ui()->DoLabel_AutoLineSize(Localize("Tee shader"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_ClCustomTeeShader, Localize("Draw tees with your own shader"), g_Config.m_ClCustomTeeShader, &Button))
	{
		g_Config.m_ClCustomTeeShader ^= 1;
	}
#if defined(CONF_PLATFORM_MACOS)
	// macOS starts on OpenGL 3.3 already, so telling a Mac player to set it is
	// telling them to restart for nothing.
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomTeeShader, &Button, Localize("Edit shader/tee.frag. The OpenGL 3.3 backend it needs is what this client already starts on."));
#else
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomTeeShader, &Button, Localize("Edit shader/tee.frag. Needs the OpenGL 3.3 backend: set gfx_backend OpenGL, gfx_gl_major 3, gfx_gl_minor 3 and restart."));
#endif
	if(g_Config.m_ClCustomTeeShader)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomTeeShaderOwn, Localize("Apply it to your own tee"), &g_Config.m_ClCustomTeeShaderOwn, &LeftView, LINE_SIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomTeeShaderOthers, Localize("Apply it to other tees"), &g_Config.m_ClCustomTeeShaderOthers, &LeftView, LINE_SIZE);
	}

	// ***** Avatar ***** //
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	Ui()->DoLabel_AutoLineSize(Localize("Picture instead of the tee"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomAvatar, Localize("Draw a round picture instead of the tee body"), &g_Config.m_ClCustomAvatar, &LeftView, LINE_SIZE);
	if(g_Config.m_ClCustomAvatar)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomAvatarOwn, Localize("Use it for your own tee"), &g_Config.m_ClCustomAvatarOwn, &LeftView, LINE_SIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomAvatarOthers, Localize("Use it for other tees as well"), &g_Config.m_ClCustomAvatarOthers, &LeftView, LINE_SIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomAvatarHideEyes, Localize("Hide the tee eyes"), &g_Config.m_ClCustomAvatarHideEyes, &LeftView, LINE_SIZE);

		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClCustomAvatarSize, &g_Config.m_ClCustomAvatarSize, &Button, Localize("Picture size"), 50, 200, &CUi::ms_LinearScrollbarScale, 0u, "%");
	}

	Ui()->DoLabel_AutoLineSize(Localize("Preview"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &RightView, HEADLINE_HEIGHT);
	RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);
	RightView.HSplitTop(70.0f, &Button, &RightView);
	RenderLeviathanTeePreview(&Button);

	if(g_Config.m_ClCustomAvatar)
	{
		// Picture list
		Ui()->DoLabel_AutoLineSize(Localize("Picture"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &RightView, HEADLINE_HEIGHT);
		RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);

		if(!m_AvatarListLoaded)
			RefreshAvatarList();

		CUIRect RefreshButton;
		RightView.HSplitBottom(20.0f, &RightView, &RefreshButton);
		RightView.HSplitBottom(MARGIN_SMALL, &RightView, nullptr);

		if(m_vAvatarNames.empty())
		{
			CUIRect Empty;
			RightView.HSplitTop(40.0f, &Empty, &RightView);
			TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
			SLabelProperties Props;
			Props.m_MaxWidth = Empty.w;
			Ui()->DoLabel(&Empty, Localize("Put .png files into the 'avatars' folder of your config directory."), 12.0f, TEXTALIGN_TL, Props);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		else
		{
			int Selected = 0;
			for(size_t i = 0; i < m_vAvatarNames.size(); ++i)
			{
				if(str_comp(m_vAvatarNames[i].c_str(), g_Config.m_ClCustomAvatarFile) == 0)
				{
					Selected = (int)i + 1;
					break;
				}
			}

			static CListBox s_ListBox;
			s_ListBox.DoStart(20.0f, (int)m_vAvatarNames.size() + 1, 1, 3, Selected, &RightView);

			{
				static int s_NoneId;
				const CListboxItem Item = s_ListBox.DoNextItem(&s_NoneId, Selected == 0);
				if(Item.m_Visible)
				{
					CUIRect Label = Item.m_Rect;
					Label.VMargin(MARGIN_SMALL, &Label);
					Ui()->DoLabel(&Label, Localize("No image"), 14.0f, TEXTALIGN_ML);
				}
			}

			for(size_t i = 0; i < m_vAvatarNames.size(); ++i)
			{
				const CListboxItem Item = s_ListBox.DoNextItem(&m_vAvatarNames[i], Selected == (int)i + 1);
				if(!Item.m_Visible)
					continue;
				CUIRect Label = Item.m_Rect;
				Label.VMargin(MARGIN_SMALL, &Label);
				Ui()->DoLabel(&Label, m_vAvatarNames[i].c_str(), 14.0f, TEXTALIGN_ML);
			}

			const int NewSelected = s_ListBox.DoEnd();
			if(NewSelected != Selected)
			{
				if(NewSelected == 0)
					g_Config.m_ClCustomAvatarFile[0] = 0;
				else
					str_copy(g_Config.m_ClCustomAvatarFile, m_vAvatarNames[NewSelected - 1].c_str());
			}
		}

		static CButtonContainer s_RefreshAvatarButton;
		if(DoButton_Menu(&s_RefreshAvatarButton, Localize("Refresh"), 0, &RefreshButton))
		{
			RefreshAvatarList();
		}
	}
}


void CMenus::RenderSettingsLeviathanHook(CUIRect MainView)
{
	CUIRect LeftView, RightView;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Hook color"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	static std::vector<CButtonContainer> s_vHookModeButtons(4);
	DoLine_RadioMenu(LeftView, Localize("Recolor the hook of"),
		s_vHookModeButtons,
		{Localize("Nobody"), Localize("Everyone"), Localize("Yourself"), Localize("Other players")},
		{0, 1, 2, 3},
		g_Config.m_ClCustomHookColor);

	if(g_Config.m_ClCustomHookColor != 0)
	{
		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomHookColorHead, Localize("Also recolor the hook head"), &g_Config.m_ClCustomHookColorHead, &LeftView, LINE_SIZE);

		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		static CButtonContainer s_HookColor;
		static int s_HookOpacity;
		DoLeviathanColorLine(&s_HookColor, &s_HookOpacity, &LeftView, Localize("Hook chain color"),
			&g_Config.m_ClCustomHookColorValue, DefaultConfig::ClCustomHookColorValue);

		CUIRect Button;
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClCustomHookColorBrightness, &g_Config.m_ClCustomHookColorBrightness, &Button, Localize("Brightness"), 20, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");
		GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomHookColorBrightness, &Button, Localize("The hook art is very dark, so the recolored hook is drawn from a brightened copy. Lower this to tone it down."));
	}
}

namespace {
struct SScanData
{
	std::vector<std::string> *m_pvNames;
	// When set, only files with this extension are listed and it is stripped
	// from the stored name.
	const char *m_pExtension;
};

// A folder given a custom icon in the Finder holds a file called "Icon" with a
// trailing carriage return, which is not something anybody meant to put there.
// Nothing the client reads has a control character in its name.
bool IsSystemJunk(const char *pName)
{
	for(const char *pCur = pName; *pCur != '\0'; ++pCur)
	{
		if((unsigned char)*pCur < 0x20)
			return true;
	}
	return false;
}

int ScanFolderCallback(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pData = static_cast<SScanData *>(pUser);
	if(IsDir || pName[0] == '.' || IsSystemJunk(pName))
		return 0;
	if(pData->m_pExtension != nullptr)
	{
		const char *pFound = str_endswith_nocase(pName, pData->m_pExtension);
		if(pFound == nullptr)
			return 0;
		pData->m_pvNames->emplace_back(pName, pFound - pName);
	}
	else
	{
		pData->m_pvNames->emplace_back(pName);
	}
	return 0;
}

void ScanFolder(IStorage *pStorage, const char *pFolder, const char *pExtension, std::vector<std::string> &vNames)
{
	vNames.clear();
	SScanData Data{&vNames, pExtension};
	pStorage->ListDirectory(IStorage::TYPE_ALL, pFolder, ScanFolderCallback, &Data);
	std::sort(vNames.begin(), vNames.end());
	// The same file can exist in several storage paths.
	vNames.erase(std::unique(vNames.begin(), vNames.end()), vNames.end());
}
} // namespace

void CMenus::RefreshCrosshairList()
{
	ScanFolder(Storage(), "crosshairs", ".png", m_vCrosshairNames);
	m_CrosshairListLoaded = true;
}

void CMenus::RefreshBackgroundList()
{
	// Backgrounds keep their extension, it decides how the file is decoded.
	ScanFolder(Storage(), "backgrounds", nullptr, m_vBackgroundNames);
	m_BackgroundListLoaded = true;
}

void CMenus::RefreshAvatarList()
{
	ScanFolder(Storage(), "avatars", ".png", m_vAvatarNames);
	m_AvatarListLoaded = true;
}

void CMenus::RefreshSoundPackList()
{
	// A sound pack is a folder, not a file.
	m_vSoundPackNames.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, "sounds", [](const char *pName, int IsDir, int DirType, void *pUser) -> int {
		// Unzipping an archive that was made on a Mac leaves a __MACOSX folder
		// beside the real one, which is not a sound pack and must not be offered
		// as one.
		if(!IsDir || pName[0] == '.' || str_comp(pName, "__MACOSX") == 0)
			return 0;
		static_cast<std::vector<std::string> *>(pUser)->emplace_back(pName);
		return 0;
	},
		&m_vSoundPackNames);
	std::sort(m_vSoundPackNames.begin(), m_vSoundPackNames.end());
	m_vSoundPackNames.erase(std::unique(m_vSoundPackNames.begin(), m_vSoundPackNames.end()), m_vSoundPackNames.end());
	m_SoundPackListLoaded = true;
}

void CMenus::RenderSettingsLeviathanCrosshair(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Custom crosshair"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_ClCustomCrosshair, Localize("Use a custom crosshair image"), g_Config.m_ClCustomCrosshair, &Button))
	{
		g_Config.m_ClCustomCrosshair ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomCrosshair, &Button, Localize("Takes precedence over the crosshair of the selected asset pack"));

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClCustomCrosshairSize, &g_Config.m_ClCustomCrosshairSize, &Button, Localize("Size"), 8, 256);

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	static CButtonContainer s_CrosshairColor;
	static int s_CrosshairOpacity;
	DoLeviathanColorLine(&s_CrosshairColor, &s_CrosshairOpacity, &LeftView, Localize("Tint color"),
		&g_Config.m_ClCustomCrosshairColor, DefaultConfig::ClCustomCrosshairColor);

	// Image list
	Ui()->DoLabel_AutoLineSize(Localize("Image"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &RightView, HEADLINE_HEIGHT);
	RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);

	if(!m_CrosshairListLoaded)
		RefreshCrosshairList();

	CUIRect RefreshButton;
	RightView.HSplitBottom(20.0f, &RightView, &RefreshButton);
	RightView.HSplitBottom(MARGIN_SMALL, &RightView, nullptr);

	if(m_vCrosshairNames.empty())
	{
		CUIRect Hint;
		RightView.HSplitTop(40.0f, &Hint, &RightView);
		TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
		SLabelProperties Props;
		Props.m_MaxWidth = Hint.w;
		Ui()->DoLabel(&Hint, Localize("Put .png files into the 'crosshairs' folder of your config directory."), 12.0f, TEXTALIGN_TL, Props);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else
	{
		// Entry 0 is "None", the rest are the found files.
		int Selected = 0;
		for(size_t i = 0; i < m_vCrosshairNames.size(); ++i)
		{
			if(str_comp(m_vCrosshairNames[i].c_str(), g_Config.m_ClCustomCrosshairFile) == 0)
			{
				Selected = (int)i + 1;
				break;
			}
		}
		// The configured file no longer exists, otherwise the list would show it
		// as selected while the config still points at it.

		static CListBox s_ListBox;
		s_ListBox.DoStart(20.0f, m_vCrosshairNames.size() + 1, 1, 3, Selected, &RightView);

		{
			static int s_NoneId;
			const CListboxItem Item = s_ListBox.DoNextItem(&s_NoneId, Selected == 0);
			if(Item.m_Visible)
			{
				CUIRect Label = Item.m_Rect;
				Label.VMargin(MARGIN_SMALL, &Label);
				Ui()->DoLabel(&Label, Localize("No image"), 14.0f, TEXTALIGN_ML);
			}
		}

		for(size_t i = 0; i < m_vCrosshairNames.size(); ++i)
		{
			const CListboxItem Item = s_ListBox.DoNextItem(&m_vCrosshairNames[i], Selected == (int)i + 1);
			if(!Item.m_Visible)
				continue;
			CUIRect Label = Item.m_Rect;
			Label.VMargin(MARGIN_SMALL, &Label);
			Ui()->DoLabel(&Label, m_vCrosshairNames[i].c_str(), 14.0f, TEXTALIGN_ML);
		}

		const int NewSelected = s_ListBox.DoEnd();
		if(NewSelected != Selected)
		{
			if(NewSelected == 0)
				g_Config.m_ClCustomCrosshairFile[0] = '\0';
			else
				str_copy(g_Config.m_ClCustomCrosshairFile, m_vCrosshairNames[NewSelected - 1].c_str());
		}
	}

	static CButtonContainer s_RefreshButton;
	if(DoButton_Menu(&s_RefreshButton, Localize("Refresh"), 0, &RefreshButton))
	{
		RefreshCrosshairList();
		// Also drop the cached texture, so that a replaced image or one that
		// failed to load earlier is picked up again.
		GameClient()->m_Hud.InvalidateCustomCrosshair();
	}
}

void CMenus::RenderSettingsLeviathanTiles(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Tile colors"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_ClCustomTileColors, Localize("Color the game layer tiles"), g_Config.m_ClCustomTileColors, &Button))
	{
		g_Config.m_ClCustomTileColors ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomTileColors, &Button, Localize("Works independently from the entities overlay. Set a color to fully transparent to hide that tile type."));
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomTileColorsFront, Localize("Also color the front layer"), &g_Config.m_ClCustomTileColorsFront, &LeftView, LINE_SIZE);

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	struct
	{
		const char *m_pLabel;
		unsigned *m_pValue;
		unsigned m_Default;
	} aColors[] = {
		{Localize("Freeze"), &g_Config.m_ClCustomTileColorFreeze, DefaultConfig::ClCustomTileColorFreeze},
		{Localize("Unfreeze"), &g_Config.m_ClCustomTileColorUnfreeze, DefaultConfig::ClCustomTileColorUnfreeze},
		{Localize("Deep freeze"), &g_Config.m_ClCustomTileColorDeepFreeze, DefaultConfig::ClCustomTileColorDeepFreeze},
		{Localize("Deep unfreeze"), &g_Config.m_ClCustomTileColorDeepUnfreeze, DefaultConfig::ClCustomTileColorDeepUnfreeze},
		{Localize("Live freeze"), &g_Config.m_ClCustomTileColorLiveFreeze, DefaultConfig::ClCustomTileColorLiveFreeze},
		{Localize("Live unfreeze"), &g_Config.m_ClCustomTileColorLiveUnfreeze, DefaultConfig::ClCustomTileColorLiveUnfreeze},
		{Localize("Kill tiles"), &g_Config.m_ClCustomTileColorDeath, DefaultConfig::ClCustomTileColorDeath},
		{Localize("Hookable"), &g_Config.m_ClCustomTileColorHookable, DefaultConfig::ClCustomTileColorHookable},
		{Localize("Unhookable"), &g_Config.m_ClCustomTileColorUnhookable, DefaultConfig::ClCustomTileColorUnhookable},
		{Localize("Hookthrough"), &g_Config.m_ClCustomTileColorThrough, DefaultConfig::ClCustomTileColorThrough},
		{Localize("Laser blocker"), &g_Config.m_ClCustomTileColorNoLaser, DefaultConfig::ClCustomTileColorNoLaser},
	};

	static CButtonContainer s_aTileColorButtons[std::size(aColors)];
	static int s_aTileColorOpacity[std::size(aColors)];
	// The first half goes into the left column, the rest into the right one.
	const size_t Half = (std::size(aColors) + 1) / 2;
	for(size_t i = 0; i < std::size(aColors); ++i)
	{
		CUIRect *pView = i < Half ? &LeftView : &RightView;
		if(i == Half)
		{
			// Skip past the height of the headline and the two checkboxes of the
			// left column, so that both columns of color pickers start at the
			// same height.
			RightView.HSplitTop(HEADLINE_HEIGHT + MARGIN_SMALL + 2.0f * LINE_SIZE + MARGIN_SMALL, nullptr, &RightView);
		}
		DoLeviathanColorLine(&s_aTileColorButtons[i], &s_aTileColorOpacity[i], pView,
			aColors[i].m_pLabel, aColors[i].m_pValue, aColors[i].m_Default);
	}
}

void CMenus::RenderSettingsLeviathanUnfreeze(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Unfreeze shot"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	const int Enabled = g_Config.m_ClUnfreeze != 0;
	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_ClUnfreeze, Localize("Look for the shot that unfreezes you"), Enabled, &Button))
	{
		g_Config.m_ClUnfreeze = Enabled ? 0 : 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClUnfreeze, &Button, Localize("The shot has to leave before the freeze, since a frozen tee cannot fire, and it can only come back to you after it has bounced off a wall."));

	if(g_Config.m_ClUnfreeze != 0)
	{
		// The mode is one setting with three values, so the second box needs an
		// id of its own rather than the address of the config variable.
		static int s_AutomaticId;
		const int Automatic = g_Config.m_ClUnfreeze == 2;
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		if(DoButton_CheckBox(&s_AutomaticId, Localize("Take the shot by itself"), Automatic, &Button))
		{
			g_Config.m_ClUnfreeze = Automatic ? 1 : 2;
		}
		GameClient()->m_Tooltips.DoToolTip(&s_AutomaticId, &Button, Localize("Aiming and firing on their own count as a bot on the official servers. Left off, bind a key to unfreeze_shoot and take the shot yourself."));

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClUnfreezeSwitchWeapon, Localize("Switch to the laser for it"), &g_Config.m_ClUnfreezeSwitchWeapon, &LeftView, LINE_SIZE);

		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		Ui()->DoLabel_AutoLineSize(Localize("Search"), 13.0f, TEXTALIGN_ML, &LeftView, LINE_SIZE);

		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClUnfreezeSteps, &g_Config.m_ClUnfreezeSteps, &Button, Localize("Aim angles tried"), 120, 1440, &CUi::ms_LinearScrollbarScale);
		GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClUnfreezeSteps, &Button, Localize("More angles find shots that need a tighter aim, and cost more while you play."));

		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClUnfreezeBounces, &g_Config.m_ClUnfreezeBounces, &Button, Localize("Bounces followed"), 4, 40, &CUi::ms_LinearScrollbarScale);
		GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClUnfreezeBounces, &Button, Localize("Every bounce buys the shot eight more ticks of life. Set it low and the shot is gone long before the freeze has carried you off the tiles."));

		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClUnfreezeHorizon, &g_Config.m_ClUnfreezeHorizon, &Button, Localize("Flight looked ahead"), 60, 300, &CUi::ms_LinearScrollbarScale, 0u, Localize(" ticks"));

		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClUnfreezeInterval, &g_Config.m_ClUnfreezeInterval, &Button, Localize("Searched every"), 40, 500, &CUi::ms_LinearScrollbarScale, 0u, Localize(" ms"));

		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClUnfreezeBudget, &g_Config.m_ClUnfreezeBudget, &Button, Localize("Search may take"), 1, 20, &CUi::ms_LinearScrollbarScale, 0u, Localize(" ms"));
		GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClUnfreezeBudget, &Button, Localize("The search stops at this and keeps the best shot it has found. This is what a heavy setting is allowed to cost you in frames."));

		Ui()->DoLabel_AutoLineSize(Localize("Drawing"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &RightView, HEADLINE_HEIGHT);
		RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClUnfreezeShowStatus, Localize("Say on screen what it is doing"), &g_Config.m_ClUnfreezeShowStatus, &RightView, LINE_SIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClUnfreezeShowPath, Localize("Draw the path of the shot"), &g_Config.m_ClUnfreezeShowPath, &RightView, LINE_SIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClUnfreezeShowFlight, Localize("Draw where the freeze carries you"), &g_Config.m_ClUnfreezeShowFlight, &RightView, LINE_SIZE);

		RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);
		static CButtonContainer s_UnfreezeColorReset;
		static int s_UnfreezeColorOpacity;
		DoLeviathanColorLine(&s_UnfreezeColorReset, &s_UnfreezeColorOpacity, &RightView,
			Localize("Color"), &g_Config.m_ClUnfreezeColor, DefaultConfig::ClUnfreezeColor);
	}
}

void CMenus::RenderSettingsLeviathanMisc(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	// ***** Music island ***** //
	Ui()->DoLabel_AutoLineSize(Localize("Music island"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &RightView, HEADLINE_HEIGHT);
	RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);

	RightView.HSplitTop(LINE_SIZE, &Button, &RightView);
	if(DoButton_CheckBox(&g_Config.m_ClMusicIsland, Localize("Show the playing track at the top"), g_Config.m_ClMusicIsland, &Button))
	{
		g_Config.m_ClMusicIsland ^= 1;
	}
#if defined(CONF_PLATFORM_MACOS)
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClMusicIsland, &Button, Localize("Reads what the system says is playing. From macOS 15.4 on, only Spotify and Music can be read, and the first time you are asked to allow it."));
#elif defined(CONF_FAMILY_WINDOWS)
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClMusicIsland, &Button, Localize("Reads the Windows media session, so it works with any player: Spotify, a browser, the system player."));
#else
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClMusicIsland, &Button, Localize("This system publishes nothing the client can read, so the island stays hidden here."));
#endif
	if(g_Config.m_ClMusicIsland)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMusicIslandIngame, Localize("Show it while playing"), &g_Config.m_ClMusicIslandIngame, &RightView, LINE_SIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMusicIslandMenu, Localize("Show it in the menus"), &g_Config.m_ClMusicIslandMenu, &RightView, LINE_SIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMusicIslandWhenPaused, Localize("Keep it visible when paused"), &g_Config.m_ClMusicIslandWhenPaused, &RightView, LINE_SIZE);

		RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);
		RightView.HSplitTop(LINE_SIZE, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_ClMusicIslandSize, &g_Config.m_ClMusicIslandSize, &Button, Localize("Size"), 50, 200, &CUi::ms_LinearScrollbarScale, 0u, "%");
		RightView.HSplitTop(LINE_SIZE, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_ClMusicIslandOpacity, &g_Config.m_ClMusicIslandOpacity, &Button, Localize("Opacity"), 10, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

		RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);
		Ui()->DoLabel_AutoLineSize(Localize("Position"), 13.0f, TEXTALIGN_ML, &RightView, LINE_SIZE);
		RightView.HSplitTop(LINE_SIZE, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_ClMusicIslandX, &g_Config.m_ClMusicIslandX, &Button, Localize("Left to right"), 0, 1000);
		RightView.HSplitTop(LINE_SIZE, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_ClMusicIslandY, &g_Config.m_ClMusicIslandY, &Button, Localize("Top to bottom"), 0, 1000);
		GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClMusicIslandY, &Button, Localize("The island is drawn in the menus too, so you can see it move while dragging these."));

		RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);
		RightView.HSplitTop(LINE_SIZE, &Button, &RightView);
		Button.VSplitLeft(Button.w / 2.0f, &Button, nullptr);
		static CButtonContainer s_ResetIslandPos;
		if(DoButton_Menu(&s_ResetIslandPos, Localize("Reset position"), 0, &Button))
		{
			g_Config.m_ClMusicIslandX = DefaultConfig::ClMusicIslandX;
			g_Config.m_ClMusicIslandY = DefaultConfig::ClMusicIslandY;
		}
	}

	Ui()->DoLabel_AutoLineSize(Localize("Spinning tee"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_ClCustomSpin, Localize("Spin for other players"), g_Config.m_ClCustomSpin, &Button))
	{
		g_Config.m_ClCustomSpin ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomSpin, &Button, Localize("Only the aim direction sent to the server rotates. Your own crosshair and view stay where you aim."));

	if(g_Config.m_ClCustomSpin)
	{
		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClCustomSpinSpeed, &g_Config.m_ClCustomSpinSpeed, &Button, Localize("Speed"), -3600, 3600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "Р’В°/s");

		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		if(DoButton_CheckBox(&g_Config.m_ClCustomSpinPauseOnAction, Localize("Stop spinning while hooking or shooting"), g_Config.m_ClCustomSpinPauseOnAction, &Button))
		{
			g_Config.m_ClCustomSpinPauseOnAction ^= 1;
		}
		GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomSpinPauseOnAction, &Button, Localize("Keep this on, otherwise your hook and your shots fly into a random direction."));

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomSpinDummy, Localize("Spin the dummy as well"), &g_Config.m_ClCustomSpinDummy, &LeftView, LINE_SIZE);
	}
}

void CMenus::RenderLeviathanTeePreview(const CUIRect *pRect)
{
	const CSkin *pDefaultSkin = GameClient()->m_Skins.Find("default");
	if(pDefaultSkin == nullptr)
		return;

	const char *pSkinName = g_Config.m_ClPlayerSkin[0] == '\0' ? "default" : g_Config.m_ClPlayerSkin;
	const CSkins::CSkinContainer *pContainer = GameClient()->m_Skins.FindContainerOrNullptr(pSkinName);

	CTeeRenderInfo Info;
	Info.Apply(pContainer == nullptr || pContainer->Skin() == nullptr ? pDefaultSkin : pContainer->Skin().get());
	Info.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
	Info.m_Size = 50.0f;
	if(g_Config.m_ClCustomOutline && g_Config.m_ClCustomOutlineOwn)
		Info.m_TeeRenderFlags |= TEE_CUSTOM_OUTLINE;
	if(g_Config.m_ClCustomAvatar && g_Config.m_ClCustomAvatarOwn)
	{
		// The component only reloads while rendering the world, so refresh here
		// as well to make the preview follow the selection right away.
		GameClient()->m_Players.UpdateAvatar();
		Info.m_AvatarTexture = GameClient()->m_Players.AvatarTexture();
	}

	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &Info, OffsetToMid);
	const vec2 TeeRenderPos = vec2(pRect->x + pRect->w / 2.0f, pRect->y + pRect->h / 2.0f + OffsetToMid.y);
	RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
}

void CMenus::RenderSettingsLeviathanBackground(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	// The menu's own moving background, which is a map rather than a picture, and
	// belongs next to the picture settings rather than three pages away under the
	// general options where nobody looks for it.
	CUIRect Themes, MenuColor;
	LeftView.HSplitBottom(150.0f, &LeftView, &Themes);
	LeftView.HSplitBottom(MARGIN_SMALL, &LeftView, nullptr);
	LeftView.HSplitBottom(COLOR_PICKER_LINE_SIZE, &LeftView, &MenuColor);
	LeftView.HSplitBottom(MARGIN_SMALL, &LeftView, nullptr);

	Ui()->DoLabel_AutoLineSize(Localize("Custom background"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomBackground, Localize("Use a custom background"), &g_Config.m_ClCustomBackground, &LeftView, LINE_SIZE);
	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_ClCustomBackgroundIngame, Localize("Show it while playing"), g_Config.m_ClCustomBackgroundIngame, &Button))
	{
		g_Config.m_ClCustomBackgroundIngame ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomBackgroundIngame, &Button, Localize("The map is drawn on top, so this is only visible where the map is see-through, for example with the entities overlay."));
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomBackgroundMenu, Localize("Show it in the menus"), &g_Config.m_ClCustomBackgroundMenu, &LeftView, LINE_SIZE);

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClCustomBackgroundOpacity, &g_Config.m_ClCustomBackgroundOpacity, &Button, Localize("Opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClCustomBackgroundVideoLength, &g_Config.m_ClCustomBackgroundVideoLength, &Button, Localize("Video length"), 0, 60, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "s");
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomBackgroundVideoLength, &Button, Localize("A longer video is cut here and starts over. 0 plays the whole file."));

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	static std::vector<CButtonContainer> s_vFitButtons(3);
	DoLine_RadioMenu(LeftView, Localize("Fill mode"),
		s_vFitButtons,
		{Localize("Stretch"), Localize("Cover"), Localize("Fit")},
		{0, 1, 2},
		g_Config.m_ClCustomBackgroundFit);

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	CUIRect Hint;
	LeftView.HSplitTop(48.0f, &Hint, &LeftView);
	TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
	SLabelProperties HintProps;
	HintProps.m_MaxWidth = Hint.w;
	// Which formats work is decided by the codecs the system ships with, so the
	// two lists genuinely differ: avi and wmv are Media Foundation's, heic and
	// mov are what macOS brings.
#if defined(CONF_PLATFORM_MACOS)
	Ui()->DoLabel(&Hint, Localize("Pictures (png, jpg, bmp, webp, heic) and videos (mp4, mov, m4v) all work."), 11.0f, TEXTALIGN_TL, HintProps);
#elif defined(CONF_FAMILY_WINDOWS)
	Ui()->DoLabel(&Hint, Localize("Pictures (png, jpg, bmp, webp) and videos (mp4, avi, wmv) all work."), 11.0f, TEXTALIGN_TL, HintProps);
#else
	Ui()->DoLabel(&Hint, Localize("Pictures (png) work. Anything else needs an FFmpeg build with decoders beside the client."), 11.0f, TEXTALIGN_TL, HintProps);
#endif
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	// File list
	Ui()->DoLabel_AutoLineSize(Localize("File"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &RightView, HEADLINE_HEIGHT);
	RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);

	if(!m_BackgroundListLoaded)
		RefreshBackgroundList();

	CUIRect RefreshButton;
	RightView.HSplitBottom(20.0f, &RightView, &RefreshButton);
	RightView.HSplitBottom(MARGIN_SMALL, &RightView, nullptr);

	if(m_vBackgroundNames.empty())
	{
		CUIRect Empty;
		RightView.HSplitTop(40.0f, &Empty, &RightView);
		TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
		SLabelProperties Props;
		Props.m_MaxWidth = Empty.w;
		Ui()->DoLabel(&Empty, Localize("Put images or videos into the 'backgrounds' folder of your config directory."), 12.0f, TEXTALIGN_TL, Props);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else
	{
		int Selected = 0;
		for(size_t i = 0; i < m_vBackgroundNames.size(); ++i)
		{
			if(str_comp(m_vBackgroundNames[i].c_str(), g_Config.m_ClCustomBackgroundFile) == 0)
			{
				Selected = (int)i + 1;
				break;
			}
		}

		static CListBox s_ListBox;
		s_ListBox.DoStart(20.0f, (int)m_vBackgroundNames.size() + 1, 1, 3, Selected, &RightView);

		{
			static int s_NoneId;
			const CListboxItem Item = s_ListBox.DoNextItem(&s_NoneId, Selected == 0);
			if(Item.m_Visible)
			{
				CUIRect Label = Item.m_Rect;
				Label.VMargin(MARGIN_SMALL, &Label);
				Ui()->DoLabel(&Label, Localize("No image"), 14.0f, TEXTALIGN_ML);
			}
		}

		for(size_t i = 0; i < m_vBackgroundNames.size(); ++i)
		{
			const CListboxItem Item = s_ListBox.DoNextItem(&m_vBackgroundNames[i], Selected == (int)i + 1);
			if(!Item.m_Visible)
				continue;
			CUIRect Label = Item.m_Rect;
			Label.VMargin(MARGIN_SMALL, &Label);
			Ui()->DoLabel(&Label, m_vBackgroundNames[i].c_str(), 14.0f, TEXTALIGN_ML);
		}

		const int NewSelected = s_ListBox.DoEnd();
		if(NewSelected != Selected)
		{
			if(NewSelected == 0)
				g_Config.m_ClCustomBackgroundFile[0] = '\0';
			else
				str_copy(g_Config.m_ClCustomBackgroundFile, m_vBackgroundNames[NewSelected - 1].c_str());
		}
	}

	static CButtonContainer s_RefreshButton;
	if(DoButton_Menu(&s_RefreshButton, Localize("Refresh"), 0, &RefreshButton))
	{
		RefreshBackgroundList();
	}
	// The colour of the menus themselves: the panels, the buttons, the bars. It
	// is a setting the game has always had and has never had a picker for, so
	// until now it could only be changed by typing ui_color into the console with
	// a packed number after it.
	static CButtonContainer s_MenuColor;
	static int s_MenuColorOpacity;
	DoLeviathanColorLine(&s_MenuColor, &s_MenuColorOpacity, &MenuColor, Localize("Menu color"),
		&g_Config.m_UiColor, 0xE4A046AF);

	// Underneath everything: the theme the menu map is picked from. The list is
	// the game's own, so a theme chosen here is the one chosen in the general
	// settings, not a second setting that says the same thing.
	Ui()->DoLabel_AutoLineSize(Localize("Menu background"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &Themes, HEADLINE_HEIGHT);
	Themes.HSplitTop(HEADLINE_HEIGHT + MARGIN_SMALL, nullptr, &Themes);
	RenderThemeSelection(Themes);

}

void CMenus::RenderSettingsLeviathanSounds(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Sound pack"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	CUIRect Hint;
	LeftView.HSplitTop(72.0f, &Hint, &LeftView);
	TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
	SLabelProperties HintProps;
	HintProps.m_MaxWidth = Hint.w;
	Ui()->DoLabel(&Hint, Localize("A pack is a folder inside 'sounds' with files named after the game sounds, for example hook_attach_ground.wav, hammer_hit.wav or gun_fire.wav. Supported formats: wav, opus, wv."), 11.0f, TEXTALIGN_TL, HintProps);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	Ui()->DoLabel_AutoLineSize(Localize("Player join and leave"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_ClCustomSoundJoin, Localize("Play a sound when a player joins"), g_Config.m_ClCustomSoundJoin, &Button))
	{
		g_Config.m_ClCustomSoundJoin ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomSoundJoin, &Button, Localize("Put player_join.wav into the pack folder"));
	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_ClCustomSoundLeave, Localize("Play a sound when a player leaves"), g_Config.m_ClCustomSoundLeave, &Button))
	{
		g_Config.m_ClCustomSoundLeave ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomSoundLeave, &Button, Localize("Put player_leave.wav into the pack folder"));

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClCustomSoundEventVolume, &g_Config.m_ClCustomSoundEventVolume, &Button, Localize("Volume"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

	// Pack list
	Ui()->DoLabel_AutoLineSize(Localize("Pack"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &RightView, HEADLINE_HEIGHT);
	RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);

	if(!m_SoundPackListLoaded)
		RefreshSoundPackList();

	CUIRect RefreshButton;
	RightView.HSplitBottom(20.0f, &RightView, &RefreshButton);
	RightView.HSplitBottom(MARGIN_SMALL, &RightView, nullptr);

	if(m_vSoundPackNames.empty())
	{
		CUIRect Empty;
		RightView.HSplitTop(40.0f, &Empty, &RightView);
		TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
		SLabelProperties Props;
		Props.m_MaxWidth = Empty.w;
		Ui()->DoLabel(&Empty, Localize("Create a folder inside the 'sounds' folder of your config directory."), 12.0f, TEXTALIGN_TL, Props);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else
	{
		int Selected = 0;
		for(size_t i = 0; i < m_vSoundPackNames.size(); ++i)
		{
			if(str_comp(m_vSoundPackNames[i].c_str(), g_Config.m_ClCustomSoundPack) == 0)
			{
				Selected = (int)i + 1;
				break;
			}
		}

		static CListBox s_ListBox;
		s_ListBox.DoStart(20.0f, (int)m_vSoundPackNames.size() + 1, 1, 3, Selected, &RightView);

		{
			static int s_NoneId;
			const CListboxItem Item = s_ListBox.DoNextItem(&s_NoneId, Selected == 0);
			if(Item.m_Visible)
			{
				CUIRect Label = Item.m_Rect;
				Label.VMargin(MARGIN_SMALL, &Label);
				Ui()->DoLabel(&Label, Localize("Default sounds"), 14.0f, TEXTALIGN_ML);
			}
		}

		for(size_t i = 0; i < m_vSoundPackNames.size(); ++i)
		{
			const CListboxItem Item = s_ListBox.DoNextItem(&m_vSoundPackNames[i], Selected == (int)i + 1);
			if(!Item.m_Visible)
				continue;
			CUIRect Label = Item.m_Rect;
			Label.VMargin(MARGIN_SMALL, &Label);
			Ui()->DoLabel(&Label, m_vSoundPackNames[i].c_str(), 14.0f, TEXTALIGN_ML);
		}

		const int NewSelected = s_ListBox.DoEnd();
		if(NewSelected != Selected)
		{
			if(NewSelected == 0)
				g_Config.m_ClCustomSoundPack[0] = 0;
			else
				str_copy(g_Config.m_ClCustomSoundPack, m_vSoundPackNames[NewSelected - 1].c_str());
		}
	}

	static CButtonContainer s_RefreshSoundButton;
	if(DoButton_Menu(&s_RefreshSoundButton, Localize("Refresh"), 0, &RefreshButton))
	{
		RefreshSoundPackList();
	}
}

void CMenus::RefreshGameAssetList()
{
	// An asset pack is either <name>.png or a folder <name> holding game.png.
	m_vGameAssetNames.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/game", [](const char *pName, int IsDir, int DirType, void *pUser) -> int {
		auto *pvNames = static_cast<std::vector<std::string> *>(pUser);
		if(pName[0] == '.')
			return 0;
		if(IsDir)
		{
			pvNames->emplace_back(pName);
			return 0;
		}
		const char *pExtension = str_endswith_nocase(pName, ".png");
		if(pExtension != nullptr)
			pvNames->emplace_back(pName, pExtension - pName);
		return 0;
	},
		&m_vGameAssetNames);
	std::sort(m_vGameAssetNames.begin(), m_vGameAssetNames.end());
	m_vGameAssetNames.erase(std::unique(m_vGameAssetNames.begin(), m_vGameAssetNames.end()), m_vGameAssetNames.end());
	m_GameAssetListLoaded = true;
}

void CMenus::RenderSettingsLeviathanModels(CUIRect MainView)
{
	struct SGroup
	{
		const char *m_pLabel;
		char *m_pValue;
		size_t m_ValueSize;
	};
	const SGroup aGroups[] = {
		{Localize("Hook"), g_Config.m_ClCustomAssetHook, sizeof(g_Config.m_ClCustomAssetHook)},
		{Localize("Hammer"), g_Config.m_ClCustomAssetHammer, sizeof(g_Config.m_ClCustomAssetHammer)},
		{Localize("Gun"), g_Config.m_ClCustomAssetGun, sizeof(g_Config.m_ClCustomAssetGun)},
		{Localize("Shotgun"), g_Config.m_ClCustomAssetShotgun, sizeof(g_Config.m_ClCustomAssetShotgun)},
		{Localize("Grenade"), g_Config.m_ClCustomAssetGrenade, sizeof(g_Config.m_ClCustomAssetGrenade)},
		{Localize("Laser"), g_Config.m_ClCustomAssetLaser, sizeof(g_Config.m_ClCustomAssetLaser)},
		{Localize("Ninja"), g_Config.m_ClCustomAssetNinja, sizeof(g_Config.m_ClCustomAssetNinja)},
		{Localize("Pickups"), g_Config.m_ClCustomAssetPickups, sizeof(g_Config.m_ClCustomAssetPickups)},
	};
	static int s_SelectedGroup = 0;
	s_SelectedGroup = std::clamp(s_SelectedGroup, 0, (int)std::size(aGroups) - 1);

	CUIRect LeftView, RightView;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Model"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	CUIRect Hint;
	LeftView.HSplitBottom(48.0f, &LeftView, &Hint);
	TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
	SLabelProperties HintProps;
	HintProps.m_MaxWidth = Hint.w;
	Ui()->DoLabel(&Hint, Localize("Pick a model on the left and the texture pack it should come from on the right. Everything else keeps using the pack from the Assets page."), 11.0f, TEXTALIGN_TL, HintProps);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	static CListBox s_GroupListBox;
	s_GroupListBox.DoStart(22.0f, (int)std::size(aGroups), 1, 3, s_SelectedGroup, &LeftView);
	for(size_t i = 0; i < std::size(aGroups); ++i)
	{
		const CListboxItem Item = s_GroupListBox.DoNextItem(aGroups[i].m_pLabel, s_SelectedGroup == (int)i);
		if(!Item.m_Visible)
			continue;
		CUIRect Label = Item.m_Rect;
		Label.VMargin(MARGIN_SMALL, &Label);
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "%s: %s", aGroups[i].m_pLabel,
			aGroups[i].m_pValue[0] == 0 ? Localize("from the Assets page") : aGroups[i].m_pValue);
		Ui()->DoLabel(&Label, aBuf, 13.0f, TEXTALIGN_ML);
	}
	s_SelectedGroup = s_GroupListBox.DoEnd();
	s_SelectedGroup = std::clamp(s_SelectedGroup, 0, (int)std::size(aGroups) - 1);

	// Pack list for the selected model
	Ui()->DoLabel_AutoLineSize(Localize("Texture pack"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &RightView, HEADLINE_HEIGHT);
	RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);

	if(!m_GameAssetListLoaded)
		RefreshGameAssetList();

	CUIRect RefreshButton;
	RightView.HSplitBottom(20.0f, &RightView, &RefreshButton);
	RightView.HSplitBottom(MARGIN_SMALL, &RightView, nullptr);

	char *pValue = aGroups[s_SelectedGroup].m_pValue;
	const size_t ValueSize = aGroups[s_SelectedGroup].m_ValueSize;

	int Selected = 0;
	for(size_t i = 0; i < m_vGameAssetNames.size(); ++i)
	{
		if(str_comp(m_vGameAssetNames[i].c_str(), pValue) == 0)
		{
			Selected = (int)i + 1;
			break;
		}
	}

	static CListBox s_PackListBox;
	s_PackListBox.DoStart(20.0f, (int)m_vGameAssetNames.size() + 1, 1, 3, Selected, &RightView);
	{
		static int s_DefaultId;
		const CListboxItem Item = s_PackListBox.DoNextItem(&s_DefaultId, Selected == 0);
		if(Item.m_Visible)
		{
			CUIRect Label = Item.m_Rect;
			Label.VMargin(MARGIN_SMALL, &Label);
			Ui()->DoLabel(&Label, Localize("from the Assets page"), 14.0f, TEXTALIGN_ML);
		}
	}
	for(size_t i = 0; i < m_vGameAssetNames.size(); ++i)
	{
		const CListboxItem Item = s_PackListBox.DoNextItem(&m_vGameAssetNames[i], Selected == (int)i + 1);
		if(!Item.m_Visible)
			continue;
		CUIRect Label = Item.m_Rect;
		Label.VMargin(MARGIN_SMALL, &Label);
		Ui()->DoLabel(&Label, m_vGameAssetNames[i].c_str(), 14.0f, TEXTALIGN_ML);
	}
	const int NewSelected = s_PackListBox.DoEnd();
	if(NewSelected != Selected)
	{
		if(NewSelected == 0)
			pValue[0] = 0;
		else
			str_copy(pValue, m_vGameAssetNames[NewSelected - 1].c_str(), ValueSize);
		// The sprites are baked into one image at load time, so rebuild it.
		GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
	}

	static CButtonContainer s_RefreshAssetButton;
	if(DoButton_Menu(&s_RefreshAssetButton, Localize("Refresh"), 0, &RefreshButton))
	{
		RefreshGameAssetList();
	}
}

// One list per side: the people you like on the left, the people you do not on
// the right. Both are the game's own lists, so anything done here shows up in
// the server browser and on the name plates at once.
void CMenus::RenderSettingsLeviathanFriends(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Row;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	struct SColumn
	{
		IFriends *m_pList;
		const char *m_pTitle;
		const char *m_pAddLabel;
		ColorRGBA m_Color;
		CUIRect m_View;
	};
	SColumn aColumns[2] = {
		{GameClient()->Friends(), Localize("Friends"), Localize("Add friend"), ColorRGBA(0.35f, 0.9f, 0.35f), LeftView},
		{GameClient()->Foes(), Localize("At war with"), Localize("Declare war"), ColorRGBA(0.95f, 0.25f, 0.25f), RightView}};

	static CLineInputBuffered<MAX_NAME_LENGTH> s_aNameInputs[2];
	static CButtonContainer s_aAddButtons[2];
	// One remove button per row, and the rows are rebuilt every frame, so the ids
	// have to be stable across frames rather than tied to the entry.
	static CButtonContainer s_aaRemoveButtons[2][16];
	static CScrollRegion s_aScrollRegions[2];

	for(int Side = 0; Side < 2; ++Side)
	{
		SColumn &Column = aColumns[Side];
		CUIRect View = Column.m_View;

		Ui()->DoLabel_AutoLineSize(Column.m_pTitle, HEADLINE_FONT_SIZE, TEXTALIGN_ML, &View, HEADLINE_HEIGHT);
		View.HSplitTop(MARGIN_SMALL, nullptr, &View);

		// Adding one. The name has to be written the way it appears in game,
		// because that is what the list matches against.
		View.HSplitTop(LINE_SIZE, &Row, &View);
		Row.VSplitRight(100.0f, &Button, &Row);
		Ui()->DoEditBox(&s_aNameInputs[Side], &Button, 12.0f);
		Row.VSplitRight(MARGIN_SMALL, &Row, nullptr);
		if(DoButton_Menu(&s_aAddButtons[Side], Column.m_pAddLabel, 0, &Row) && !s_aNameInputs[Side].IsEmpty())
		{
			Column.m_pList->AddFriend(s_aNameInputs[Side].GetString(), "");
			s_aNameInputs[Side].Clear();
			FriendlistOnUpdate();
			Client()->ServerBrowserUpdate();
		}

		View.HSplitTop(MARGIN_SMALL, nullptr, &View);

		CUIRect ListView = View;
		CScrollRegionParams Params;
		Params.m_ScrollUnit = 60.0f;
		s_aScrollRegions[Side].Begin(&ListView, &Params);

		int Shown = 0;
		for(int i = 0; i < Column.m_pList->NumFriends(); ++i)
		{
			const CFriendInfo *pInfo = Column.m_pList->GetFriend(i);
			if(pInfo == nullptr || pInfo->m_aName[0] == 0)
				continue;

			ListView.HSplitTop(LINE_SIZE, &Row, &ListView);
			s_aScrollRegions[Side].AddRect(Row);

			CUIRect Dot, Name, Remove;
			Row.VSplitLeft(14.0f, &Dot, &Name);
			Name.VSplitRight(70.0f, &Name, &Remove);

			TextRender()->TextColor(Column.m_Color);
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			Ui()->DoLabel(&Dot, FontIcon::CIRCLE, 8.0f, TEXTALIGN_MC);
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			Ui()->DoLabel(&Name, pInfo->m_aName, 12.0f, TEXTALIGN_ML);

			if(Shown < (int)std::size(s_aaRemoveButtons[Side]))
			{
				if(DoButton_Menu(&s_aaRemoveButtons[Side][Shown], Localize("Remove"), 0, &Remove, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 3.0f, 0.0f, ColorRGBA(0.4f, 0.15f, 0.15f, 0.4f)))
				{
					Column.m_pList->RemoveFriend(pInfo->m_aName, pInfo->m_aClan);
					FriendlistOnUpdate();
					Client()->ServerBrowserUpdate();
					break;
				}
			}
			++Shown;
		}
		s_aScrollRegions[Side].End();

		if(Shown == 0)
		{
			CUIRect Empty = View;
			Empty.HSplitTop(LINE_SIZE, &Empty, nullptr);
			TextRender()->TextColor(ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f));
			Ui()->DoLabel(&Empty, Localize("Nobody yet"), 12.0f, TEXTALIGN_ML);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}

	// The one setting that belongs here, under both lists.
	CUIRect Bottom;
	MainView.HSplitBottom(LINE_SIZE, nullptr, &Bottom);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRelationDots, Localize("Show the dot by their name in game"), &g_Config.m_ClRelationDots, &Bottom, LINE_SIZE);
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClRelationDots, &Bottom, Localize("Green for a friend, red for war, nothing for everybody else. Say !war <name> in the chat to declare war without coming here."));
}

// Where the key that flips focus mode currently lives, found by looking rather
// than stored: the bind list is the truth, and a copy of it would drift.
static CBindSlot FocusModeBind(CBinds *pBinds)
{
	for(int Modifier = 0; Modifier < KeyModifier::COMBINATION_COUNT; ++Modifier)
	{
		for(int Key = KEY_FIRST; Key < KEY_LAST; ++Key)
		{
			const char *pBind = pBinds->Get(Key, Modifier);
			if(pBind != nullptr && str_comp(pBind, "toggle_focus_mode") == 0)
				return CBindSlot(Key, Modifier);
		}
	}
	return EMPTY_BIND_SLOT;
}

void CMenus::RenderSettingsLeviathanTrail(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Focus mode"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &RightView, HEADLINE_HEIGHT);
	RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFocusMode, Localize("Focus mode is on"), &g_Config.m_ClFocusMode, &RightView, LINE_SIZE);
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClFocusMode, &RightView, Localize("Strips the screen down to the game. What goes is picked below; the key flips it without coming here."));

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFocusHideNames, Localize("Hide the name plates"), &g_Config.m_ClFocusHideNames, &RightView, LINE_SIZE);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFocusHideEffects, Localize("Hide particles and damage stars"), &g_Config.m_ClFocusHideEffects, &RightView, LINE_SIZE);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFocusHideHud, Localize("Hide the HUD"), &g_Config.m_ClFocusHideHud, &RightView, LINE_SIZE);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFocusHideMusic, Localize("Hide the music island"), &g_Config.m_ClFocusHideMusic, &RightView, LINE_SIZE);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFocusHideExtra, Localize("Hide broadcasts and the kill feed"), &g_Config.m_ClFocusHideExtra, &RightView, LINE_SIZE);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFocusHideChat, Localize("Hide the chat"), &g_Config.m_ClFocusHideChat, &RightView, LINE_SIZE);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFocusHideScoreboard, Localize("Hide the scoreboard"), &g_Config.m_ClFocusHideScoreboard, &RightView, LINE_SIZE);

	RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);
	RightView.HSplitTop(LINE_SIZE, &Button, &RightView);
	CUIRect KeyLabel, KeyReader;
	Button.VSplitMid(&KeyLabel, &KeyReader, MARGIN_SMALL);
	Ui()->DoLabel(&KeyLabel, Localize("Focus mode key"), 13.0f, TEXTALIGN_ML);
	const CBindSlot CurrentBind = FocusModeBind(&GameClient()->m_Binds);
	static CButtonContainer s_FocusKeyReader, s_FocusKeyClear;
	const CKeyBinder::CKeyReaderResult KeyResult = GameClient()->m_KeyBinder.DoKeyReader(
		&s_FocusKeyReader, &s_FocusKeyClear, &KeyReader, CurrentBind, false);
	if(!KeyResult.m_Aborted && KeyResult.m_Bind != CurrentBind)
	{
		if(CurrentBind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(CurrentBind.m_Key, "", false, CurrentBind.m_ModifierMask);
		if(KeyResult.m_Bind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(KeyResult.m_Bind.m_Key, "toggle_focus_mode", false, KeyResult.m_Bind.m_ModifierMask);
	}

	Ui()->DoLabel_AutoLineSize(Localize("Tee trail"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClTeeTrail, Localize("Draw a trail behind tees"), &g_Config.m_ClTeeTrail, &LeftView, LINE_SIZE);
	if(g_Config.m_ClTeeTrail)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClTeeTrailOthers, Localize("Other tees leave one too"), &g_Config.m_ClTeeTrailOthers, &LeftView, LINE_SIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClTeeTrailFade, Localize("Fade it out toward the end"), &g_Config.m_ClTeeTrailFade, &LeftView, LINE_SIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClTeeTrailTaper, Localize("Narrow it toward the end"), &g_Config.m_ClTeeTrailTaper, &LeftView, LINE_SIZE);

		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		static std::vector<CButtonContainer> s_vModeButtons(4);
		DoLine_RadioMenu(LeftView, Localize("Colored by"),
			s_vModeButtons,
			{Localize("Color"), Localize("Tee"), Localize("Rainbow"), Localize("Speed")},
			{0, 1, 2, 3}, g_Config.m_ClTeeTrailMode);

		if(g_Config.m_ClTeeTrailMode == 0)
		{
			static CButtonContainer s_TrailColor;
			LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
			DoLine_ColorPicker(&s_TrailColor, COLOR_PICKER_LINE_SIZE, COLOR_PICKER_LABEL_SIZE, 0.0f, &LeftView,
				Localize("Trail color"), &g_Config.m_ClTeeTrailColor, DefaultColor(0xFFFFFF), false, nullptr, false);
		}

		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClTeeTrailWidth, &g_Config.m_ClTeeTrailWidth, &Button, Localize("Width"), 2, 40, &CUi::ms_LinearScrollbarScale);
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClTeeTrailLength, &g_Config.m_ClTeeTrailLength, &Button, Localize("Length"), 5, 150, &CUi::ms_LinearScrollbarScale, 0u, Localize(" ticks"));
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClTeeTrailAlpha, &g_Config.m_ClTeeTrailAlpha, &Button, Localize("Opacity"), 5, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");
	}
}
