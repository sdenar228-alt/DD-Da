/* Custom client additions.
 *
 * This file is included from `config_variables.h` (multiple times, X-macro
 * style) and must therefore NOT have an include guard.
 *
 * All variables here use the `cl_custom_` prefix so they can never collide
 * with a variable that upstream DDNet might add later. The official DDNet
 * client stores unknown config lines verbatim (see
 * `CConfigManager::StoreUnknownCommand`) and writes them back out on save, so
 * these settings survive a round trip through the Steam version of the game
 * untouched.
 */

// --- tee outline -----------------------------------------------------------
MACRO_CONFIG_INT(ClCustomOutline, cl_custom_outline, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw tees with a custom colored outline")
MACRO_CONFIG_COL(ClCustomOutlineColor, cl_custom_outline_color, 4278190080, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of the custom tee outline")
MACRO_CONFIG_INT(ClCustomOutlineSize, cl_custom_outline_size, 115, 100, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the custom tee outline in percent")
MACRO_CONFIG_INT(ClCustomOutlineOwn, cl_custom_outline_own, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply the custom outline to your own tee")
MACRO_CONFIG_INT(ClCustomOutlineOthers, cl_custom_outline_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply the custom outline to other tees")

// --- hook ------------------------------------------------------------------
MACRO_CONFIG_INT(ClCustomHookColor, cl_custom_hook_color, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Recolor the hook chain (0 = off, 1 = all tees, 2 = own hook only, 3 = other tees only)")
MACRO_CONFIG_COL(ClCustomHookColorValue, cl_custom_hook_color_value, 4294967295, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of the hook chain")
MACRO_CONFIG_INT(ClCustomHookColorHead, cl_custom_hook_color_head, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also recolor the hook head, not just the chain")

// --- crosshair -------------------------------------------------------------
MACRO_CONFIG_INT(ClCustomCrosshair, cl_custom_crosshair, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use a custom crosshair image instead of the one from the assets")
MACRO_CONFIG_STR(ClCustomCrosshairFile, cl_custom_crosshair_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Crosshair image inside the 'crosshairs' folder, without the .png extension")
MACRO_CONFIG_INT(ClCustomCrosshairSize, cl_custom_crosshair_size, 64, 8, 256, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the custom crosshair")
MACRO_CONFIG_COL(ClCustomCrosshairColor, cl_custom_crosshair_color, 4294967295, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Tint color of the custom crosshair")

// --- spinning tee ----------------------------------------------------------
MACRO_CONFIG_INT(ClCustomSpin, cl_custom_spin, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spin the aim direction that other players see. Your own view and aim are not affected")
MACRO_CONFIG_INT(ClCustomSpinSpeed, cl_custom_spin_speed, 720, -3600, 3600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spin speed in degrees per second, negative values spin the other way")
MACRO_CONFIG_INT(ClCustomSpinPauseOnAction, cl_custom_spin_pause_on_action, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Send the real aim direction while hooking or firing, so hook and shots go where you aim")
MACRO_CONFIG_INT(ClCustomSpinDummy, cl_custom_spin_dummy, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also spin the dummy")

// --- tile colors -----------------------------------------------------------
// Colored overlay for the game layer, independent from `cl_overlay_entities`.
// Every color has an alpha channel; alpha 0 means the tile type is not drawn.
MACRO_CONFIG_INT(ClCustomTileColors, cl_custom_tile_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw a colored overlay over the game layer tiles")
MACRO_CONFIG_INT(ClCustomTileColorsFront, cl_custom_tile_colors_front, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also color tiles of the front layer")
MACRO_CONFIG_COL(ClCustomTileColorHookable, cl_custom_tile_color_hookable, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of hookable blocks")
MACRO_CONFIG_COL(ClCustomTileColorUnhookable, cl_custom_tile_color_unhookable, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of unhookable blocks")
MACRO_CONFIG_COL(ClCustomTileColorDeath, cl_custom_tile_color_death, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of kill tiles")
MACRO_CONFIG_COL(ClCustomTileColorFreeze, cl_custom_tile_color_freeze, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of freeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorUnfreeze, cl_custom_tile_color_unfreeze, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of unfreeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorDeepFreeze, cl_custom_tile_color_deep_freeze, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of deep freeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorDeepUnfreeze, cl_custom_tile_color_deep_unfreeze, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of deep unfreeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorLiveFreeze, cl_custom_tile_color_live_freeze, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of live freeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorLiveUnfreeze, cl_custom_tile_color_live_unfreeze, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of live unfreeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorNoLaser, cl_custom_tile_color_nolaser, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of laser blocker tiles")
MACRO_CONFIG_COL(ClCustomTileColorThrough, cl_custom_tile_color_through, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of hookthrough tiles")
