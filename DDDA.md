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

### For an iPhone

DDNet carries a full iOS port, so the fork builds for a phone as it is. Xcode is
macOS only, so the build runs on a GitHub Actions macOS runner instead: the
**iOS app** workflow can be started by hand from the Actions tab and leaves a
`DDDa.ipa` as its artifact. It fetches `ddnet-libs` itself, because this
repository does not carry it, and it boots the simulator build once and checks
that the client actually starts before it packages the device build.

That `.ipa` is unsigned, and an unsigned app cannot be installed on an iPhone by
any means. Signing it needs an Apple ID: [Sideloadly](https://sideloadly.io)
does it from Windows over a cable, for free, and the app then stops launching
after seven days and has to be signed again. A paid Apple developer account
lasts a year and can hand the build over through TestFlight instead, with no
cable and nothing installed on the phone but Apple's own app.

Two features are missing there. The music island reads the Windows media
session, which has no equivalent an app is allowed to read on iOS, and the
background falls back to PNG only: the iOS build has no FFmpeg and no Media
Foundation, so videos and jpg do not load. Everything else, the unfreeze module
and the tee shader included, works the same.

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

Anything larger than 1280x720 is therefore asked for at 720p while it is being
decoded rather than at its own size. The decoder scales it as part of the work it
is already doing, and everything after that point, the copy out of the reader,
the swizzle to RGBA and the upload, is done on a ninth of the pixels a 4K file
would otherwise cost. Sources that refuse to be scaled on the way out are taken
at their own size and say so in the log. A background that is fully opaque is
also drawn with blending switched off, since a screen sized quad with nothing
underneath it to mix with is worth frames on a weaker card.

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

### Unfreeze shot

Works out how to unfreeze yourself with your own laser, and either draws the
shot or takes it.

Three rules of the game decide everything about this feature. A frozen tee
cannot fire, so the shot has to leave *before* the freeze. A laser cannot touch
the tee that fired it until it has hit a wall, and it only moves once every
`laser_bounce_delay`, so the earliest it can come back is eight ticks later at
the default tuning. And a tee unfrozen while it is still touching freeze tiles
is frozen again in the same tick, for the full `sv_freeze_delay`, so the hit is
only worth anything at a moment where the tee is frozen *and* off the tiles.

So the module predicts the tee's own flight first. It copies the client's
predicted world, cuts the copy loose from the original, and ticks it forward
`cl_unfreeze_horizon` ticks, which gives the position, the freeze timer and the
tiles touched on every tick ahead. A frozen tee ignores its input, so that
flight is exact as long as nobody hooks you.

Then it sweeps `cl_unfreeze_steps` aim angles. Each one is traced the way the
laser itself bounces: the same wall test, the same axis mirror off the surface,
the same energy spent per bounce. A candidate wins when one of its bounce
stretches passes within a tee's radius of the predicted flight at a tick that is
worth hitting. Neighbouring ticks are checked as well, and the shot that
survives being a tick early or late is preferred, because one lost input message
moves everything by a tick.

`cl_unfreeze 1` only draws the plan: the path of the shot, a marker where it
would hit, and the flight itself while frozen. `unfreeze_shoot` takes that shot,
so it can go on a key:

```
bind mouse3 unfreeze_shoot
```

`cl_unfreeze 2` also fires it. Aiming and firing by itself is what the official
servers call a bot, so it is off by default and the setting says so.

The aim is written onto the copy of the input that is sent, never onto the
stored one, for the same reason the spinning tee does that. The shot itself is a
step of two on the fire counter, which is one press and one release without
touching the parity, so your own fire key stays in step and the laser's full
automatic mode is not armed by accident. The plan is re-checked against the tick
it would actually leave on before every shot, since a plan made two ticks ago
aims at where you were going to be two ticks ago.

Deep freeze is never attempted: a laser does not lift it, the tile puts it back
every tick. Live freeze is left alone for the same reason.

The search only runs while a freeze is actually coming: predicting the flight
costs about a third of a millisecond, and the angle sweep, which is the
expensive half at a few milliseconds, is skipped entirely when there is nothing
worth hitting ahead. `cl_unfreeze_interval` decides how often it may run at all,
and `cl_unfreeze_steps` trades the cost against how tight an aim it can find.

`cl_unfreeze_bounces` is the setting that decides whether the module can do
anything at all. Every bounce buys the shot eight more ticks of life, and the
freeze usually needs twenty to forty ticks to carry the tee off the tiles, so a
shot followed for only a few bounces is always dead by the time it would matter.
That is why the lowest it can be set to is four, and why the default is sixteen.

The map's own tuning is followed rather than the stock physics. The flight runs
in a copy of the predicted world, so tune zones, speedups and everything else
the tiles do to a tee apply to it, and it starts from the tee's real velocity.
The shot reads two tunings, the way the game does: how far it reaches comes from
the zone the tee is standing in, while the bounce delay, the bounce count and
the bounce cost come from the zone the shot is fired in, sampled once at the
muzzle.

Copies of the predicted world are cut loose from the original before they are
ticked. Both entity links have to be cleared, not just the parent: the copy
constructor carries the source's child pointer along, that pointer aims into a
world the client may have thrown away already, and removing an entity writes
through it. Leaving it in place corrupts the heap, which surfaces later as an
assertion somewhere else entirely, usually in the snapshot code.

A shot is only a plan if it survives the way the game actually resolves it, so
the search follows the beam the way the game does rather than looking only at the
stretch it likes:

* A laser ends at the **first tee it touches**, whoever that is. So every stretch
  is checked in order against the flight and against the other tees near it, and
  the first crossing is where the shot stops. If that crossing is not a tick
  worth hitting, the angle is dead rather than a plan; following it further would
  be describing a beam that no longer exists. This is what used to make the
  module fire shots that came back through the player before the freeze.
* The **clock starts one tick before the input is stamped**, because the server
  runs a fresh input the moment the packet lands rather than on the tick the
  client wrote on it. Every bounce is counted from there, and each one is matched
  against the position a tee published at the end of the previous tick.
* The aim is sent as **whole units**, so the angle that is traced is the one that
  the integer target actually produces, not the one that was wanted.
* Two bounces in a row that cover no ground kill a laser, which is how the game
  stops a beam trapped in a corner. The trace does the same, so it cannot plan on
  bounces that never happen.
* Another tee only eats the shot when it stands **closer along the beam** than
  the point where the shot would have hit its owner. A tee behind that point is
  behind a beam that has already ended.

The angle is not the only thing swept. Bounces land every `laser_bounce_delay`
worth of ticks and nowhere in between, so waiting a tick before firing moves the
whole ladder of bounces by a tick, and only a handful of delays can ever put a
bounce inside the window that is worth hitting. Those are worked out from the
window itself and only they are traced. It costs a few times more than a single
sweep, and it is the difference between finding a plan for a flight and having
none, because a window is often three or four ticks wide while the bounces are
eight apart. A plan made for one fire tick is only fired on that tick: firing it
a tick later aims at where the tee was going to be a tick earlier.

What a plan is worth is measured in **ticks of freeze it takes off**: the hit is
scored against what would have happened anyway, so a shot that frees the tee a
moment before it would have thawed by itself is worth nothing and is not taken.
Eight ticks is the least that counts there. A window that ends because a tile
freezes the tee again is judged by a different rule and needs only two, because
those ticks are not comfort, they are control handed back: two ticks is a hook or
a jump, and taking them is what gets the tee out of the pit instead of riding it
to the bottom. Among plans of similar worth the one with room for error wins, and
the aim is moved to the middle of the band of angles that work rather than the
edge the sweep happened to find.

The whole chain was measured against the game's own laser rather than trusted.
The module's plan was fed to a real `CLaser` inside the same simulation, across
five maps of very different geometry and 580 predicted flights: open rooms,
corridors, tight tunnels and vertical shafts, each spot flown ten ways, from a
standing drop to a sixteen unit fling. Thirty five of those flights had a window
worth shooting into at all. The module planned a shot for sixteen of them, all
sixteen lifted the freeze, and all sixteen did it on exactly the tick the plan
had named. Of the nineteen it refused, eighteen were refusals a brute force sweep
agreed with, there being no angle and no fire delay that would have worked, which
leaves one flight it should have found and did not.

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

