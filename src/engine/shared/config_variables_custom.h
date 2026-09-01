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
MACRO_CONFIG_INT(ClCustomHookColorBrightness, cl_custom_hook_color_brightness, 100, 20, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Brightness of the recolored hook in percent")

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

// --- custom tee shader -----------------------------------------------------
MACRO_CONFIG_INT(ClCustomTeeShader, cl_custom_tee_shader, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw tees with the shader from shader/tee.vert and shader/tee.frag (OpenGL backend only)")
MACRO_CONFIG_INT(ClCustomTeeShaderOwn, cl_custom_tee_shader_own, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply the tee shader to your own tee")
MACRO_CONFIG_INT(ClCustomTeeShaderOthers, cl_custom_tee_shader_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply the tee shader to other tees")

// --- per weapon asset packs ------------------------------------------------
// Each of these names an asset pack from 'assets/game'. The sprites of that
// group are copied out of it into the pack selected with `cl_asset_game`, so
// single models can be mixed from several downloaded texture packs.
MACRO_CONFIG_STR(ClCustomAssetHook, cl_custom_asset_hook, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Asset pack the hook is taken from")
MACRO_CONFIG_STR(ClCustomAssetHammer, cl_custom_asset_hammer, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Asset pack the hammer is taken from")
MACRO_CONFIG_STR(ClCustomAssetGun, cl_custom_asset_gun, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Asset pack the gun is taken from")
MACRO_CONFIG_STR(ClCustomAssetShotgun, cl_custom_asset_shotgun, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Asset pack the shotgun is taken from")
MACRO_CONFIG_STR(ClCustomAssetGrenade, cl_custom_asset_grenade, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Asset pack the grenade launcher is taken from")
MACRO_CONFIG_STR(ClCustomAssetLaser, cl_custom_asset_laser, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Asset pack the laser rifle is taken from")
MACRO_CONFIG_STR(ClCustomAssetNinja, cl_custom_asset_ninja, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Asset pack the ninja is taken from")
MACRO_CONFIG_STR(ClCustomAssetPickups, cl_custom_asset_pickups, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Asset pack the pickups and the health/armor bars are taken from")

// --- music island ----------------------------------------------------------
MACRO_CONFIG_INT(ClMusicIsland, cl_music_island, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show what is playing in a pill at the top of the screen")
MACRO_CONFIG_INT(ClMusicIslandIngame, cl_music_island_ingame, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the music island while playing")
MACRO_CONFIG_INT(ClMusicIslandMenu, cl_music_island_menu, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the music island in the menus")
MACRO_CONFIG_INT(ClMusicIslandSize, cl_music_island_size, 100, 50, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the music island in percent")
MACRO_CONFIG_INT(ClMusicIslandX, cl_music_island_x, 500, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Horizontal position of the music island, 0 is left and 1000 is right")
MACRO_CONFIG_INT(ClMusicIslandY, cl_music_island_y, 0, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Vertical position of the music island, 0 is top and 1000 is bottom")
MACRO_CONFIG_INT(ClMusicIslandOpacity, cl_music_island_opacity, 100, 10, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of the music island in percent")
MACRO_CONFIG_INT(ClMusicIslandWhenPaused, cl_music_island_when_paused, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keep the island visible while the music is paused")

// --- sounds ----------------------------------------------------------------
MACRO_CONFIG_STR(ClCustomSoundPack, cl_custom_sound_pack, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Sound pack folder inside 'sounds'")
MACRO_CONFIG_INT(ClCustomSoundJoin, cl_custom_sound_join, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Play a sound when a player joins the server")
MACRO_CONFIG_INT(ClCustomSoundLeave, cl_custom_sound_leave, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Play a sound when a player leaves the server")
MACRO_CONFIG_INT(ClCustomSoundEventVolume, cl_custom_sound_event_volume, 60, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Volume of the join and leave sounds in percent")

// --- avatar instead of the tee body ----------------------------------------
MACRO_CONFIG_INT(ClCustomAvatar, cl_custom_avatar, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw a round picture instead of the tee body")
MACRO_CONFIG_STR(ClCustomAvatarFile, cl_custom_avatar_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Picture inside the 'avatars' folder, without the .png extension")
MACRO_CONFIG_INT(ClCustomAvatarOwn, cl_custom_avatar_own, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use the avatar for your own tee")
MACRO_CONFIG_INT(ClCustomAvatarOthers, cl_custom_avatar_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use the avatar for other tees as well")
MACRO_CONFIG_INT(ClCustomAvatarSize, cl_custom_avatar_size, 100, 50, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Avatar size relative to the tee body in percent")
MACRO_CONFIG_INT(ClCustomAvatarHideEyes, cl_custom_avatar_hide_eyes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide the tee eyes while an avatar is drawn")

// --- custom background -----------------------------------------------------
MACRO_CONFIG_INT(ClCustomBackground, cl_custom_background, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw a custom image or video behind everything")
MACRO_CONFIG_STR(ClCustomBackgroundFile, cl_custom_background_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "File inside the 'backgrounds' folder, with its extension")
MACRO_CONFIG_INT(ClCustomBackgroundOpacity, cl_custom_background_opacity, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of the custom background in percent")
MACRO_CONFIG_INT(ClCustomBackgroundFit, cl_custom_background_fit, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How the background fills the screen (0 = stretch, 1 = cover, 2 = fit)")
MACRO_CONFIG_INT(ClCustomBackgroundVideoLength, cl_custom_background_video_length, 10, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only play the first seconds of a background video and loop from there, 0 plays the whole file")
MACRO_CONFIG_INT(ClCustomBackgroundIngame, cl_custom_background_ingame, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the custom background while playing")
MACRO_CONFIG_INT(ClCustomBackgroundMenu, cl_custom_background_menu, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the custom background in the menus")

// --- tile colors -----------------------------------------------------------
// Colored overlay for the game layer, independent from `cl_overlay_entities`.
// Every color has an alpha channel; alpha 0 means the tile type is not drawn.
// Packed value is (A << 24) | (H << 16) | (S << 8) | L.
MACRO_CONFIG_INT(ClCustomTileColors, cl_custom_tile_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw a colored overlay over the game layer tiles")
MACRO_CONFIG_INT(ClCustomTileColorsFront, cl_custom_tile_colors_front, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also color tiles of the front layer")
MACRO_CONFIG_COL(ClCustomTileColorHookable, cl_custom_tile_color_hookable, 0x9900005A, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of hookable blocks")
MACRO_CONFIG_COL(ClCustomTileColorUnhookable, cl_custom_tile_color_unhookable, 0x9914C878, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of unhookable blocks")
MACRO_CONFIG_COL(ClCustomTileColorDeath, cl_custom_tile_color_death, 0x9900DC6E, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of kill tiles")
MACRO_CONFIG_COL(ClCustomTileColorFreeze, cl_custom_tile_color_freeze, 0x9996C88C, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of freeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorUnfreeze, cl_custom_tile_color_unfreeze, 0x995AB48C, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of unfreeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorDeepFreeze, cl_custom_tile_color_deep_freeze, 0x99AAC86E, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of deep freeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorDeepUnfreeze, cl_custom_tile_color_deep_unfreeze, 0x9978B482, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of deep unfreeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorLiveFreeze, cl_custom_tile_color_live_freeze, 0x9987C88C, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of live freeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorLiveUnfreeze, cl_custom_tile_color_live_unfreeze, 0x9946B48C, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of live unfreeze tiles")
MACRO_CONFIG_COL(ClCustomTileColorNoLaser, cl_custom_tile_color_nolaser, 0x99DCB478, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of laser blocker tiles")
MACRO_CONFIG_COL(ClCustomTileColorThrough, cl_custom_tile_color_through, 0x99C8B482, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of hookthrough tiles")

// Unfreeze module
MACRO_CONFIG_INT(ClUnfreeze, cl_unfreeze, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser self unfreeze: 0 off, 1 only show the shot, 2 take it as well")
MACRO_CONFIG_INT(ClUnfreezeHorizon, cl_unfreeze_horizon, 150, 60, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many ticks of your own flight the module looks ahead")
MACRO_CONFIG_INT(ClUnfreezeSteps, cl_unfreeze_steps, 360, 120, 1440, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many aim angles are tried, more finds rarer shots and costs more")
MACRO_CONFIG_INT(ClUnfreezeBounces, cl_unfreeze_bounces, 16, 4, 40, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many bounces of the shot are followed")
MACRO_CONFIG_INT(ClUnfreezeInterval, cl_unfreeze_interval, 100, 40, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How often the shot is searched for, in milliseconds")
MACRO_CONFIG_INT(ClUnfreezeBudget, cl_unfreeze_budget, 3, 1, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long one search may take, in milliseconds, before it settles for the best it has")
MACRO_CONFIG_INT(ClUnfreezeDebug, cl_unfreeze_debug, 0, 0, 1, CFGFLAG_CLIENT, "Write what the unfreeze module decides, and why, to unfreeze.log in the config folder")
MACRO_CONFIG_INT(ClUnfreezeSwitchWeapon, cl_unfreeze_switch_weapon, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Switch to the laser by yourself when a shot was found")
MACRO_CONFIG_INT(ClUnfreezeShowPath, cl_unfreeze_show_path, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw the path the shot would take")
MACRO_CONFIG_INT(ClUnfreezeShowStatus, cl_unfreeze_show_status, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Say on screen what the unfreeze module is doing")
MACRO_CONFIG_INT(ClUnfreezeShowFlight, cl_unfreeze_show_flight, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw where the freeze is going to carry you")
MACRO_CONFIG_COL(ClUnfreezeColor, cl_unfreeze_color, 0xFF55DDFF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color the unfreeze shot is drawn in")

// Discord rich presence
MACRO_CONFIG_STR(ClDiscordAppId, cl_discord_app_id, 24, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Discord application id the rich presence runs under, empty uses DDNet's")
// Focus mode: one key strips the screen down to the game, and the checkboxes
// say which parts go.
MACRO_CONFIG_INT(ClFocusMode, cl_focus_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode: hide the parts of the interface picked below")
MACRO_CONFIG_INT(ClFocusHideNames, cl_focus_hide_names, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides the name plates")
MACRO_CONFIG_INT(ClFocusHideEffects, cl_focus_hide_effects, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides particles and damage stars")
MACRO_CONFIG_INT(ClFocusHideHud, cl_focus_hide_hud, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides the HUD")
MACRO_CONFIG_INT(ClFocusHideMusic, cl_focus_hide_music, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides the music island")
MACRO_CONFIG_INT(ClFocusHideExtra, cl_focus_hide_extra, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides broadcasts and the kill feed")
MACRO_CONFIG_INT(ClFocusHideChat, cl_focus_hide_chat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides the chat, except while you are typing")
MACRO_CONFIG_INT(ClFocusHideScoreboard, cl_focus_hide_scoreboard, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides the scoreboard")

// 3D particles: wireframe shapes drifting behind the game.
MACRO_CONFIG_INT(Cl3dParticles, cl_3d_particles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Wireframe shapes drifting and tumbling behind the game")
MACRO_CONFIG_INT(Cl3dParticlesCount, cl_3d_particles_count, 20, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many of them")
MACRO_CONFIG_INT(Cl3dParticlesType, cl_3d_particles_type, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Their shape: 0 cube, 1 heart, 2 circle, 3 hexagon, 4 triangle, 5 mixed")
MACRO_CONFIG_INT(Cl3dParticlesSize, cl_3d_particles_size, 55, 10, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Their size")
MACRO_CONFIG_INT(Cl3dParticlesDepth, cl_3d_particles_depth, 16, 0, 60, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How thick the flat shapes are, in percent of their size")
MACRO_CONFIG_INT(Cl3dParticlesSpeed, cl_3d_particles_speed, 75, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How fast they drift and tumble, in percent")
MACRO_CONFIG_INT(Cl3dParticlesAlpha, cl_3d_particles_alpha, 100, 5, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Their opacity in percent")
MACRO_CONFIG_INT(Cl3dParticlesColorMode, cl_3d_particles_color_mode, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What colors them: 0 random per shape, 1 the picked color, 2 a rainbow")
MACRO_CONFIG_COL(Cl3dParticlesColor, cl_3d_particles_color, 0x55FFB4, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The picked color")
MACRO_CONFIG_INT(Cl3dParticlesGlow, cl_3d_particles_glow, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "A glow around the wireframes")
MACRO_CONFIG_INT(Cl3dParticlesGlowAlpha, cl_3d_particles_glow_alpha, 35, 5, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of the glow in percent")
MACRO_CONFIG_INT(Cl3dParticlesGlowOffset, cl_3d_particles_glow_offset, 2, 1, 10, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How far the glow reaches around the lines")

// Gradient text: the letters take their color from a hue that walks along the
// line and with the clock.
MACRO_CONFIG_INT(ClGradientTextIngame, cl_gradient_text_ingame, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient text while playing")
MACRO_CONFIG_INT(ClGradientTextMenu, cl_gradient_text_menu, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient text in the menus")
MACRO_CONFIG_INT(ClGradientTextSpeed, cl_gradient_text_speed, 50, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How fast the gradient travels, in percent")
MACRO_CONFIG_COL(ClGradientTextColor, cl_gradient_text_color, 0x00FFB4, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The hue the gradient is built around, and how saturated it runs")
MACRO_CONFIG_INT(ClGradientTextSpread, cl_gradient_text_spread, 40, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How far around that hue the gradient travels, in percent, all the way being the whole wheel")
MACRO_CONFIG_INT(ClGradientTextBrightness, cl_gradient_text_brightness, 60, 10, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How bright the letters come out, in percent")

// The settings page opens where it was left, the way the rest of the settings do.
MACRO_CONFIG_INT(UiLeviathanPage, ui_leviathan_page, 0, 0, 9, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Which Leviathan settings tab was open last")

// Auto replies and the name badge.
MACRO_CONFIG_INT(ClAutoReplyMuted, cl_auto_reply_muted, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Answer players you have muted when they address you")
MACRO_CONFIG_STR(ClAutoReplyMutedMsg, cl_auto_reply_muted_msg, 128, "I have muted you", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What the answer says")
MACRO_CONFIG_INT(ClAutoReplyAfk, cl_auto_reply_afk, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Answer when somebody addresses you while the game is not focused")
MACRO_CONFIG_STR(ClAutoReplyAfkMsg, cl_auto_reply_afk_msg, 128, "I am not tabbed in", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What the answer says")
MACRO_CONFIG_INT(ClClientBadge, cl_client_badge, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Wear the client name and logo on a plate above the game timer")
MACRO_CONFIG_INT(ClClientBadgeClock, cl_client_badge_clock, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Put the computer's own clock on the plate beside the name")

// A hat on the tee's head.
MACRO_CONFIG_INT(ClHat, cl_hat, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Wear a hat")
MACRO_CONFIG_STR(ClHatFile, cl_hat_file, 64, "crown", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Which hat, a png from the 'hats' folder")
MACRO_CONFIG_INT(ClHatSize, cl_hat_size, 100, 50, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the hat in percent")
MACRO_CONFIG_INT(ClHatOffset, cl_hat_offset, 0, -30, 30, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How much higher the hat sits")

// Fancy weapons: dressed-up beams for the rifle and the shotgun.
MACRO_CONFIG_INT(ClFancyWeapons, cl_fancy_weapons, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dress up the laser weapons: recolored beams with sparks along them")
MACRO_CONFIG_INT(ClFancyLaser, cl_fancy_laser, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The crystal laser: an icy blue beam with sparks")
MACRO_CONFIG_INT(ClFancyShotgun, cl_fancy_shotgun, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The sandy shotgun: a golden beam with sparks")

// Tee trail
MACRO_CONFIG_INT(ClTeeTrail, cl_tee_trail, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw a ribbon behind tees, tracing where they have just been")
MACRO_CONFIG_INT(ClTeeTrailOthers, cl_tee_trail_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Trail behind other tees too, not only your own")
MACRO_CONFIG_INT(ClTeeTrailFade, cl_tee_trail_fade, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fade the trail out toward its end")
MACRO_CONFIG_INT(ClTeeTrailTaper, cl_tee_trail_taper, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Narrow the trail toward its end")
MACRO_CONFIG_INT(ClTeeTrailMode, cl_tee_trail_mode, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What colors the trail: 0 the picked color, 1 the tee's color, 2 a rainbow, 3 the speed")
MACRO_CONFIG_COL(ClTeeTrailColor, cl_tee_trail_color, 0xFFFFFF, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the trail in the picked color mode")
MACRO_CONFIG_INT(ClTeeTrailWidth, cl_tee_trail_width, 15, 2, 40, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of the trail at the tee")
MACRO_CONFIG_INT(ClTeeTrailLength, cl_tee_trail_length, 25, 5, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many ticks of flight the trail covers")
MACRO_CONFIG_INT(ClTeeTrailAlpha, cl_tee_trail_alpha, 80, 5, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of the trail in percent")

MACRO_CONFIG_STR(ClDiscordAppAsset, cl_discord_app_asset, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Name of the image asset in your Discord application, empty looks for leviathan_logo")

// Friend and war markers
MACRO_CONFIG_INT(ClRelationDots, cl_relation_dots, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Green dot by the name of a friend, red by the name of someone you declared war on")
