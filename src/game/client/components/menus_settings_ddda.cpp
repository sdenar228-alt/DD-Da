/* Settings page for the client specific features. */
#include "menus.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/components/hud.h>
#include <game/client/components/skins.h>
#include <game/client/components/tooltips.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <algorithm>

enum
{
	DDDA_TAB_TEES = 0,
	DDDA_TAB_HOOK,
	DDDA_TAB_CROSSHAIR,
	DDDA_TAB_TILES,
	DDDA_TAB_MISC,
	NUMBER_OF_DDDA_TABS,
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

void CMenus::DoDDDaColorLine(CButtonContainer *pResetId, const void *pOpacityId, CUIRect *pView, const char *pLabel, unsigned *pColor, unsigned Default)
{
	DoLine_ColorPicker(pResetId, COLOR_PICKER_LINE_SIZE, COLOR_PICKER_LABEL_SIZE, 0.0f, pView,
		pLabel, pColor, DefaultColor(Default), false, nullptr, true);

	// The alpha byte of the packed HSLA value doubles as the opacity.
	const int OldOpacity = (int)(((*pColor) >> 24) & 0xFFu) * 100 / 255;
	int Opacity = OldOpacity;
	CUIRect Button;
	pView->HSplitTop(LINE_SIZE, &Button, pView);
	Button.VSplitLeft(10.0f, nullptr, &Button);
	Ui()->DoScrollbarOption(pOpacityId, &Opacity, &Button, Localize("Opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");
	if(Opacity != OldOpacity)
	{
		const unsigned Alpha = (unsigned)std::clamp(Opacity * 255 / 100, 0, 255);
		*pColor = ((*pColor) & 0x00FFFFFFu) | (Alpha << 24);
	}
	pView->HSplitTop(COLOR_PICKER_LINE_SPACING, nullptr, pView);
}

void CMenus::RenderSettingsDDDa(CUIRect MainView)
{
	static int s_CurTab = DDDA_TAB_TEES;

	CUIRect TabBar, Button;
	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / (float)NUMBER_OF_DDDA_TABS;
	static CButtonContainer s_aPageTabs[NUMBER_OF_DDDA_TABS] = {};
	const char *apTabNames[NUMBER_OF_DDDA_TABS] = {
		Localize("Tees"),
		Localize("Hook"),
		Localize("Crosshair"),
		Localize("Tiles"),
		Localize("Misc")};

	for(int Tab = 0; Tab < NUMBER_OF_DDDA_TABS; ++Tab)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == 0 ? IGraphics::CORNER_L : (Tab == NUMBER_OF_DDDA_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			s_CurTab = Tab;
		}
	}

	MainView.HSplitTop(10.0f, nullptr, &MainView);

	switch(s_CurTab)
	{
	case DDDA_TAB_TEES: RenderSettingsDDDaTees(MainView); break;
	case DDDA_TAB_HOOK: RenderSettingsDDDaHook(MainView); break;
	case DDDA_TAB_CROSSHAIR: RenderSettingsDDDaCrosshair(MainView); break;
	case DDDA_TAB_TILES: RenderSettingsDDDaTiles(MainView); break;
	case DDDA_TAB_MISC: RenderSettingsDDDaMisc(MainView); break;
	default: break;
	}
}

void CMenus::RenderSettingsDDDaTees(CUIRect MainView)
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
		DoDDDaColorLine(&s_OutlineColor, &s_OutlineOpacity, &LeftView, Localize("Outline color"),
			&g_Config.m_ClCustomOutlineColor, DefaultConfig::ClCustomOutlineColor);
	}

	Ui()->DoLabel_AutoLineSize(Localize("Preview"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &RightView, HEADLINE_HEIGHT);
	RightView.HSplitTop(MARGIN_SMALL, nullptr, &RightView);
	RightView.HSplitTop(60.0f, &Button, &RightView);
	RenderDDDaTeePreview(&Button);
}

void CMenus::RenderSettingsDDDaHook(CUIRect MainView)
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
		DoDDDaColorLine(&s_HookColor, &s_HookOpacity, &LeftView, Localize("Hook chain color"),
			&g_Config.m_ClCustomHookColorValue, DefaultConfig::ClCustomHookColorValue);

		CUIRect Button;
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClCustomHookColorBrightness, &g_Config.m_ClCustomHookColorBrightness, &Button, Localize("Brightness"), 20, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");
		GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomHookColorBrightness, &Button, Localize("The hook art is very dark, so the recolored hook is drawn from a brightened copy. Lower this to tone it down."));
	}
}

void CMenus::RefreshCrosshairList()
{
	m_vCrosshairNames.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, "crosshairs", [](const char *pName, int IsDir, int DirType, void *pUser) -> int {
		auto *pvNames = static_cast<std::vector<std::string> *>(pUser);
		if(IsDir)
			return 0;
		const char *pExtension = str_endswith(pName, ".png");
		if(pExtension == nullptr)
			return 0;
		pvNames->emplace_back(pName, pExtension - pName);
		return 0;
	},
		&m_vCrosshairNames);

	std::sort(m_vCrosshairNames.begin(), m_vCrosshairNames.end());
	// The same file can exist in several storage paths.
	m_vCrosshairNames.erase(std::unique(m_vCrosshairNames.begin(), m_vCrosshairNames.end()), m_vCrosshairNames.end());
	m_CrosshairListLoaded = true;
}

void CMenus::RenderSettingsDDDaCrosshair(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Custom crosshair"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomCrosshair, Localize("Use a custom crosshair image"), &g_Config.m_ClCustomCrosshair, &LeftView, LINE_SIZE);
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomCrosshair, &LeftView, Localize("Takes precedence over the crosshair of the selected asset pack"));

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClCustomCrosshairSize, &g_Config.m_ClCustomCrosshairSize, &Button, Localize("Size"), 8, 256);

	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
	static CButtonContainer s_CrosshairColor;
	static int s_CrosshairOpacity;
	DoDDDaColorLine(&s_CrosshairColor, &s_CrosshairOpacity, &LeftView, Localize("Tint color"),
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
		if(Selected == 0 && g_Config.m_ClCustomCrosshairFile[0] != '\0')
			g_Config.m_ClCustomCrosshairFile[0] = '\0';

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

void CMenus::RenderSettingsDDDaTiles(CUIRect MainView)
{
	CUIRect LeftView, RightView;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Tile colors"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomTileColors, Localize("Color the game layer tiles"), &g_Config.m_ClCustomTileColors, &LeftView, LINE_SIZE);
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomTileColors, &LeftView, Localize("Works independently from the entities overlay. Set a color to fully transparent to hide that tile type."));
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
		DoDDDaColorLine(&s_aTileColorButtons[i], &s_aTileColorOpacity[i], pView,
			aColors[i].m_pLabel, aColors[i].m_pValue, aColors[i].m_Default);
	}
}

void CMenus::RenderSettingsDDDaMisc(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button;
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	Ui()->DoLabel_AutoLineSize(Localize("Spinning tee"), HEADLINE_FONT_SIZE, TEXTALIGN_ML, &LeftView, HEADLINE_HEIGHT);
	LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomSpin, Localize("Spin for other players"), &g_Config.m_ClCustomSpin, &LeftView, LINE_SIZE);
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomSpin, &LeftView, Localize("Only the aim direction sent to the server rotates. Your own crosshair and view stay where you aim."));

	if(g_Config.m_ClCustomSpin)
	{
		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		LeftView.HSplitTop(LINE_SIZE, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClCustomSpinSpeed, &g_Config.m_ClCustomSpinSpeed, &Button, Localize("Speed"), -3600, 3600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "°/s");

		LeftView.HSplitTop(MARGIN_SMALL, nullptr, &LeftView);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomSpinPauseOnAction, Localize("Stop spinning while hooking or shooting"), &g_Config.m_ClCustomSpinPauseOnAction, &LeftView, LINE_SIZE);
		GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClCustomSpinPauseOnAction, &LeftView, Localize("Keep this on, otherwise your hook and your shots fly into a random direction."));

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCustomSpinDummy, Localize("Spin the dummy as well"), &g_Config.m_ClCustomSpinDummy, &LeftView, LINE_SIZE);
	}
}

void CMenus::RenderDDDaTeePreview(const CUIRect *pRect)
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

	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &Info, OffsetToMid);
	const vec2 TeeRenderPos = vec2(pRect->x + pRect->w / 2.0f, pRect->y + pRect->h / 2.0f + OffsetToMid.y);
	RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
}
