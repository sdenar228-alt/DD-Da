# Leviathan

A DDNet 20.1 fork with extra client side features. Everything is configured in
the **Leviathan** tab of the settings, or from the console (F1) with the
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

### The Telegram button

Under the other links in the start menu, opening <https://t.me/leviathanddnet>.
The address is deliberately not passed through the translations, unlike the DDNet
links above it: a translated string is something a translator may change, and an
address that changes sends people somewhere else.

### Artwork

The client's own icon lives in `other/icons/Leviathan.ico`, built with every size
Windows asks for from 16 up to 256, and is compiled into the executable. The logo
above the buttons in the start menu is `data/leviathan_logo.png`, loaded the first
time that menu is drawn; if it is missing the menu falls back to the name in
writing rather than leaving a hole. Replacing either is a matter of replacing the
file, but a new file under `data` also has to be listed in `EXPECTED_DATA` in
`CMakeLists.txt`, or it will not be copied next to the executable and the client
will show the missing texture instead.

### For an iPhone

DDNet carries a full iOS port, so the fork builds for a phone as it is. Xcode is
macOS only, so the build runs on a GitHub Actions macOS runner instead: the
**iOS app** workflow can be started by hand from the Actions tab and leaves a
`Leviathan.ipa` as its artifact. It fetches `ddnet-libs` itself, because this
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

## Friends and war

A dot in front of the name says how somebody stands with you: green for a friend,
red for someone you are at war with, nothing at all for everybody else. It rides
on the game's own friend and foe lists, so the marks survive a restart and the
same people show up in the server browser and the menus.

The **Friends** tab of the settings holds both lists side by side, green on the
left and red on the right, with a box to type a name into and a button to take
one off again. Nothing there is separate from the rest of the game: a name added
here is a friend or a foe everywhere, the browser included.

War is also declared from the chat box, which is quicker in the middle of a
round:

```
!war name
!unwar name
```

The line is caught before anything is sent, so the server never sees it; the
client answers in the chat to say it took. The name is matched the way the friend
list matches, so it has to be the player's name as it appears in game.

Friends are added the way they always were, from the scoreboard or the server
browser, and the dot follows from that.

`cl_relation_dots 0` turns the dots off and leaves the lists alone.

## Discord presence

Built in, and on. While the client is running Discord shows what you are doing:
the server's name, the map, and how many people are on it, with an "Ask to join"
button on servers that are publicly listed. On unlisted ones the party id is
random so the address does not leak.

The name and the picture Discord puts next to it belong to whoever owns the
Discord application, and by default that is DDNet's, so it says DDNet. To have it
say Leviathan, make an application at <https://discord.com/developers>, upload an
image under Rich Presence, and put the application id in `cl_discord_app_id`.
Discord looks the image up by name, so if you called it anything other than
`leviathan_logo`, put that name in `cl_discord_app_asset`. Left empty, DDNet's
application is used and everything still works, under their name.

The connection is made after the config file has been read, not while the client
is being built, because the application id is a setting and at construction time
nothing has been read yet. Changing either setting therefore takes a restart.

Under the presence there is a **Telegram** button. That is why the presence talks
to Discord over its own socket on Windows rather than through the Game SDK the
rest of DDNet uses: the SDK's activity has no field for buttons and never has.
The socket takes the activity as JSON and accepts up to two of them, needs no
library beside it, and saves shipping three and a half megabytes of DLL. The SDK
remains as a fallback and is what every other platform uses.

Discord does not show your own buttons back to you, so the only way to see one is
from somebody else's client.

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

Three fields of the input are involved, and each one belongs somewhere else.

The **shot** goes on the stored input, because the fire counter is cumulative and
the client's own bookkeeping has to stay in step with it. It is a step of two,
which is one press and one release without touching the parity, so your own fire
key stays in step and the laser's full automatic mode is not armed by accident.

The **aim** and the **weapon request** go on the copy that is sent, never on the
stored input. For the aim that is the same reason the spinning tee does it. For
the weapon it matters far more than it looks: that field is sticky, and DDNet
clears it every time you turn the wheel, because otherwise the last weapon you
picked by number would override the wheel on every tick for the rest of the
round. A module that writes it into the stored input wipes that clearing out and
takes your weapon wheel away. Restoring it is the same trap from the other side:
the field reads zero whenever you last used the wheel, so writing the saved value
back restores nothing and leaves you holding the laser, which is exactly what
looked like the client scrolling through your weapons by itself. The weapon is
therefore put back by its own number, read off the predicted character before the
switch, and the module keeps asking until the predicted character is holding it
again.

The request has to go out on more than one input as well. The server takes the
*value* from one tick and the *request* from the tick before it, so a single
input carrying the laser switches nothing.

Which tick a plan is fired on is not a detail either. The input that
`CControls::SnapInput` builds is the one the server runs on the tick
`PredGameTick()` names **at that moment**, and `SnapInput` runs before the
components render. So a plan made while rendering can never be for the tick that
has just gone out; the earliest it can name is the next one, and the module fires
it when `PredGameTick()` is that tick exactly. Off by one here and the plan is
stale on every single input, which is a module that quietly never shoots. It
therefore plans at least two ticks ahead, and eight when the laser still has to
be switched to, and if the predicted tick skips over the one the plan named, the
plan is dropped and a new search starts at once rather than after the rest of the
interval.

Deep freeze is never attempted: a laser does not lift it, the tile puts it back
every tick. Live freeze is left alone for the same reason.

The search only runs while a freeze is actually coming: predicting the flight
costs about a third of a millisecond, and the angle sweep, which is the expensive
half, is skipped entirely when there is nothing worth hitting ahead.
`cl_unfreeze_interval` decides how often it may run at all, and
`cl_unfreeze_steps` trades the cost against how tight an aim it can find.

The sweep itself is coarse first. Nearly every angle sends the beam nowhere near
the flight, so tracing all of them at the resolution the setting asks for is
almost all waste. Instead a sixth of them are traced, the handful that landed or
came closest are traced again around their neighbourhood, and the best couple of
those are traced a third time five times finer still. That reaches an angle finer
than a plain sweep of the same cost, which matters because a band wide enough to
hit a tee across a room can still be a quarter of a degree wide. Measured against
a plain sweep on the same flights it finds the same shots for a third of the
time.

Whatever the settings say, `cl_unfreeze_budget` is the most one search may cost,
three milliseconds by default. When it runs out the search keeps the best plan it
had. Without it a heavy setting is felt as a stutter every time a freeze comes
into range, and worse: a frame that runs long makes the predicted tick skip, and
the skipped tick can be the one the plan was going to fire on. The status line
shows what the last search actually took.

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

