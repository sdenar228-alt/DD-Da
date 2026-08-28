# DD-Da

A DDNet 20.1 fork with extra client side features. Everything is configured in
the **DD-Da** tab of the settings, or from the console (F1) with the
`cl_custom_*` variables.

## Building

```
build.bat
```

It finds Visual Studio through vswhere, sets up the MSVC environment and builds
with Ninja into `build\DDNet.exe`.

The Ninja generator is used on purpose: CMake copies the runtime DLLs and the
`data` folder into the build directory root, so a multi-config generator like
"Visual Studio" would put the executable into `build\Release\` where it finds
neither and fails to start with missing DLL errors.

## Config folder

New folders in `%APPDATA%\DDNet`:

| Folder | Contents |
| --- | --- |
| `avatars` | `.png` pictures that can replace the tee body |
| `backgrounds` | images and videos used as a background |
| `crosshairs` | `.png` crosshair images |
| `sounds` | one folder per sound pack |
| `shader` | overrides for the shaders in `data/shader` |

## Features

### Tees

* **Outline** вЂ” a colored outline around every tee, with adjustable thickness.
  The outline sprites in a skin are pure black with only an alpha channel, so
  the client builds a white mask per skin to make the color show up.
* **Picture instead of the tee** вЂ” a `.png` from `avatars`, masked into a circle
  like a chat profile picture. Works for your own tee, for others, or both.
* **Tee shader** вЂ” see below.

All three apply to 0.6 tees. A 0.7 skin is drawn by a separate renderer that has
no outline masks and no avatar or shader path, so on a 0.7 server the tees fall
back to the plain skin. The preview in the settings always draws a 0.6 tee, so it
keeps showing the outline there.

### Hook

Recolors the hook chain and head for nobody, everyone, only yourself or only
others. The hook art is very dark (it peaks at about 52% brightness) and the
graphics pipeline can only multiply colors down, so the recolored hook is drawn
from a brightness normalized copy of the sprites. The brightness slider tones
that back down.

### Crosshair

A `.png` from `crosshairs` with its own size and tint, taking precedence over
the crosshair of the selected asset pack.

### Tiles

A colored overlay over the game layer for freeze, unfreeze, deep and live
freeze, kill, hookable, unhookable, hookthrough and laser blocker tiles. It
works independently from the entities overlay. Setting a color to fully
transparent hides that tile type.

### Background

An image or video behind everything, in game and in the menus.

Supported on Windows without any extra download:

| Kind | Formats | Decoder |
| --- | --- | --- |
| Pictures | png | engine, all platforms |
| Pictures | jpg, jpeg, bmp, webp, tif, gif | Windows Imaging Component |
| Videos | mp4, mov, avi, wmv, m4v and whatever else the system plays | Media Foundation |

Videos are played forward at their own frame rate and loop at the end. `cl_custom_background_video_length` cuts a longer file after that many seconds (10 by default, 0 plays all of it). Every
frame is a full texture upload, so a small file is cheaper than a 4K one.

The FFmpeg shipped in `ddnet-libs` was compiled for **encoding only** and has no
decoders at all, so it is not used for this. It stays as a fallback for other
platforms; to make it work there, put a full FFmpeg build of the same major
versions (`avcodec-61`, `avformat-61`, `avutil-59`, `swresample-5`,
`swscale-8`) next to the executable.

The Media Foundation DLLs are delay loaded, so a Windows edition that ships
without them (the N editions) still starts and only fails to decode.

In game the map is drawn on top, so the background is only visible where the map
is see-through, for example with the entities overlay.

### Sounds

A sound pack is a folder inside `sounds` holding files named after the game
sound sets, for example `hook_attach_ground.wav`, `hammer_hit.wav`,
`gun_fire.wav`, `player_spawn.wav`. Supported formats are **wav**, **opus** and
**wv** вЂ” plain WAV support was added to the engine for this, so files do not
have to be converted first.

Two sounds exist that vanilla DDNet does not have: `player_join` and
`player_leave`, played when somebody joins or leaves the server.

### Models

Single weapon and pickup models can be taken from other downloaded texture
packs: pick a model on the left and the pack it should come from on the right.
The sprites are copied into one image at load time, so packs of different
resolutions are scaled to fit. Everything not overridden keeps using the pack
from the regular Assets page.

### Music island

A rounded pill showing what is playing right now, the way a phone shows it:
album art, title, artist and three bars that bounce while the track runs. It
slides in when the music starts and slides out when it stops.

The track comes from the **Windows media session**, the same source as the
volume flyout, so every player that reports to the system works: Spotify, a
browser tab, the system player. Nothing has to be configured in the player
itself. The query runs on its own thread and polls twice a second, so it costs
nothing on the render thread.

The pill holds the album art, the title, the artist, previous / play-pause /
next buttons and a progress bar along the bottom. The buttons can be clicked
wherever the mouse is a cursor, so in the menus; in game the mouse aims, and the
console commands below cover that.

There is no like button: the Windows media session exposes play, pause and
track skipping, but nothing for favouriting, that lives inside each player.

A progress bar along the bottom shows how far the track has run. Players publish
their position only now and then rather than continuously, so the reported value
is carried forward by the time since it was published, otherwise the bar would
sit still.

Three console commands drive the player that owns the session, so they work in
game where the mouse is busy aiming:

```
bind pgup music_prev
bind pgdown music_next
bind pause music_play_pause
```

`cl_music_island_x` and `cl_music_island_y` place it anywhere on the screen, in
permille of the space it can move in, so the spot stays right at any resolution.
The settings page has sliders for both and a reset button. In the menus the pill
can also be grabbed anywhere outside the three buttons and dragged, which writes
the same two settings, so a dragged island stays where it was put.

The island is drawn after everything else and reads the mouse itself instead of
going through the interface's hot item handshake. That handshake spans two
frames and is validated inside the menus, which have already closed their check
window by the time the island draws, so an activation made there would be thrown
away before the button was released. While the mouse is over the pill it claims
the hover, so a menu button underneath cannot be pressed through it.

This is Windows only. On other platforms the island simply never appears.

### Spinning tee

Rotates the aim direction that is sent to the server, so other players see the
tee spinning. Your own crosshair and view are unaffected, because the local tee
is rendered from the real mouse position. While hooking or firing the real angle
is sent, otherwise the hook and the shots would fly into a random direction.

The spin is applied to a copy of the input, never to the stored one, so a dummy
swap or an input reset cannot freeze the aim at a random angle.

## Tee shader

`cl_custom_tee_shader 1` draws tees with `data/shader/tee.vert` and
`data/shader/tee.frag` instead of the built in sprite shader. Editing the
fragment shader is enough, there is no need to rebuild the client. A copy in the
`shader` folder of the config directory takes precedence over the one in `data`.

Available uniforms:

| Name | Meaning |
| --- | --- |
| `gTextureSampler` | the tee sprite, only with `TW_TEXTURED` |
| `gVerticesColor` | the color the game asked for, alpha included |
| `gTime` | seconds since the client started, for animations |

`texCoord` and `vertColor` come in as varyings, `FragClr` is the output. The
shipped `tee.frag` reproduces the default look and has commented out rainbow and
pulse examples to start from.

**This needs the OpenGL 3.3 backend.** The default backend on Windows is legacy
OpenGL 1.1, which has no shader support at all. Set this once and restart:

```
gfx_backend OpenGL
gfx_gl_major 3
gfx_gl_minor 3
```

On Vulkan the setting is ignored and tees render normally, because Vulkan needs
precompiled SPIR-V rather than GLSL source.

## Steam

**Config compatibility.** All settings use the `cl_custom_` prefix so they can
never collide with a variable that upstream DDNet might add later. The official
client stores config lines it does not know and writes them back out on save
(`CConfigManager::StoreUnknownCommand`), so these settings survive a round trip
through the Steam version untouched. Both clients share
`%APPDATA%\DDNet\settings_ddnet.cfg`, so sensitivity, binds and the rest stay in
sync automatically.

The only upstream variable whose range changed is `ui_settings_page`, which now
allows the extra tab index; the official client clamps it back to its own range
and shows the Credits tab instead.

**Playtime.** Playtime cannot be added through code. Steam counts the runtime of
the process it launched itself under a given AppID, and no Steamworks call
exists to add hours. The only way to have this build count towards DDNet is to
put it where Steam launches DDNet from, i.e. replace `DDNet.exe` in
`steamapps\common\DDraceNetwork`.

## Antiping

Already part of upstream DDNet, nothing was added: `cl_antiping` plus
`cl_antiping_players`, `cl_antiping_grenade`, `cl_antiping_weapons`,
`cl_antiping_smooth`, `cl_antiping_gunfire`, `cl_antiping_preinput`,
`cl_antiping_limit` and `cl_antiping_percent`.

