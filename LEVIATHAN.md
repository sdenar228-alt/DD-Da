# Leviathan

A DDNet 20.1 fork with extra client side features. Everything is configured in
the **Leviathan** tab of the settings, or from the console (F1) with the
`cl_custom_*` variables.

## Building

On Windows:

```
build.bat
```

It finds Visual Studio through vswhere, sets up the MSVC environment and builds
with Ninja into `build\Leviathan.exe`.

On macOS:

```
chmod +x build.sh
./build.sh
```

It checks for the command line tools, for CMake and Ninja and for `ddnet-libs`,
and builds `build/Leviathan`. That is the bare binary, run from the build
directory. The `.app` and the disk image around it are a separate step:

```
cmake --build build --target package_dmg
```

which needs `dmgbuild` on the path *while CMake configures*, or the target is
never created at all. There is also a **macOS app** workflow that does all of
this on a GitHub runner and leaves the image as its artifact, for when there is
no Mac to hand. Nothing it produces is signed, so the copy that comes off it has
to be let out of quarantine once:

```
xattr -dr com.apple.quarantine /Applications/Leviathan.app
```

`ddnet-libs` is neither committed here nor a submodule of this repository,
whatever the leftover `.gitmodules` says, so `git submodule update --init` runs
without error and fetches nothing. Clone it in by hand:

```
git clone --depth 1 https://github.com/ddnet/ddnet-libs
```

The Ninja generator is used on purpose: CMake copies the runtime libraries and
the `data` folder into the build directory root, so a multi-config generator
like "Visual Studio" or Xcode would put the executable into a configuration
subdirectory where it finds neither and fails to start.

### The Telegram button

Under the other links in the start menu, opening <https://t.me/leviathanddnet>.
The address is deliberately not passed through the translations, unlike the DDNet
links above it: a translated string is something a translator may change, and an
address that changes sends people somewhere else.

### Artwork

The client's own icon lives in `other/icons/Leviathan.ico`, built with every size
Windows asks for from 16 up to 256, and is compiled into the executable. macOS
wants the same picture again as `other/icons/Leviathan.icns`, which carries 16
through 512 and is copied into the app's `Contents/Resources` rather than
compiled in; `CFBundleIconFile` in `other/bundle/client/Info.plist.in` is what
points at it, by name and without the extension. The logo
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

Two features are missing there, and both for the same reason: an iOS build gets
neither of the system halves. The music island has nothing to read, since an app
is not allowed at the now playing information other apps publish, and the
background falls back to PNG only, since the only decoder left is the FFmpeg in
`ddnet-libs`, which was compiled for encoding and has none. ImageIO and
AVFoundation do exist on a phone, so the macOS decoder could be built for it, but
it is not: `custom_media_mac.mm` and `custom_music_mac.mm` are added to the build
only for macOS, and iOS takes the same do-nothing stubs as Linux. Everything
else, the unfreeze module and the tee shader included, works the same.

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

## Tee trail

A ribbon behind every tee, tracing where it has just been, drawn under the
players. Its tab holds the width, the length in ticks, the opacity, whether it
fades and narrows toward the end, and what colors it: a picked color, the tee's
own color, a rainbow offset per player, or the speed, green through red. The
positions are sampled every frame from the same smoothed spot the tee itself is
drawn at, so the ribbon stays smooth at any frame rate, and a teleport cuts it
rather than streaking it across the map.

## Hats

A hat on your own tee's head: a crown, horns or a halo ship with the client, and
any png dropped into the `hats` folder of the config directory stands in the
same list. Picked at the bottom of the Tees page, with a size and a height. The
hat follows the body through its animation but stays upright, hats being hats,
and it is client side only: nobody else sees it, the same as every other
decoration here.

## Focus mode

One key that strips the screen down to the game. What goes is picked by
checkbox: the name plates, the particles and damage stars, the HUD, the music
island, the broadcasts and the kill feed, the chat, the scoreboard. The chat
never hides while it is being typed into. The key is bound from the same page
through the game's own key reader, and the command behind it is
`toggle_focus_mode`, so it can be bound from the console as well.

## Fancy weapons

Two costumes for the laser weapons, next to the weapon models in the settings:
the crystal laser, an icy blue beam, and the sandy shotgun, a golden one, both
with sparks strewn along the beam. The colors sit on top of the picked laser
colors rather than replacing them, so switching the costume off returns exactly
what was set before. The sparks are a hash of the beam and the clock: nothing is
stored, and the same beam carries the same sparks from frame to frame. The
switches preview themselves right in the settings with the same drawing the game
uses.

## 3D particles

Wireframe shapes drifting and tumbling behind the game: cubes, hearts, circles,
hexagons, triangles, or a mix. Their tab sets how many, the size, the speed, the
opacity, the color, one random per shape, a picked one, or a rainbow, and a glow
with its own opacity and reach. They live in a box the size of the view plus a
margin and wrap around its edges, so the field never thins out. Decoration only:
they draw over the map background and under everything that matters.

## Discord presence

Built in, and on. While the client is running Discord shows what you are doing:
the server's name, the map, and how many people are on it.

There is no "Ask to join" button. That was the Game SDK's, and it went when the
presence moved to the socket: the activity sent there carries no join secret, so
Discord has nothing to offer anybody. Nothing about the server leaks either, for
the same reason, and the party id is the fixed string `leviathan` rather than a
random one.

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
to Discord over its own socket rather than through the Game SDK the rest of DDNet
uses: the SDK's activity has no field for buttons and never has. The socket takes
the activity as JSON and accepts up to two of them, needs no library beside it,
and saves shipping three and a half megabytes of DLL.

Discord offers that socket as a named pipe on Windows and as a unix socket on
macOS, and the protocol above the two is the same, so the button is there on both.
The unix one is looked for in `$XDG_RUNTIME_DIR`, then `$TMPDIR`, `$TMP`, `$TEMP`
and finally `/tmp`, numbered `discord-ipc-0` through `-9`; on macOS it is the
per-user `$TMPDIR` that answers. The SDK remains as a fallback for the platforms
neither transport covers.

Discord does not show your own buttons back to you, so the only way to see one is
from somebody else's client.

## Config folder

New folders in the config directory, which is `%APPDATA%\DDNet` on Windows and
`~/Library/Application Support/DDNet` on macOS:

| Folder | Contents |
| --- | --- |
| `avatars` | `.png` pictures that can replace the tee body |
| `backgrounds` | images and videos used as a background |
| `crosshairs` | `.png` crosshair images |
| `sounds` | one folder per sound pack |
| `shader` | overrides for the shaders in `data/shader` |

A crosshair or an avatar is stored in the settings without its extension and
read back with a lowercase `.png`. A background keeps its full filename, because
the extension is what decides which decoder gets it. So on a case sensitive
volume, which macOS can be formatted as but is not by default, a `CROSSHAIR.PNG`
is listed and then fails to load with a line in the log, while a background of
any spelling is fine. Lowercase extensions avoid it.

Entries the Finder leaves behind are skipped rather than offered: a `__MACOSX`
folder from an unzipped pack is rejected by name, and the `Icon` file a folder
with a custom picture carries is rejected by the carriage return in its name,
along with anything else holding a control character.

## Features

### Tees

* **Outline** — a colored outline around every tee, with adjustable thickness.
  The outline sprites in a skin are pure black with only an alpha channel, so
  the client builds a white mask per skin to make the color show up.
* **Picture instead of the tee** — a `.png` from `avatars`, masked into a circle
  like a chat profile picture. Works for your own tee, for others, or both.
* **Tee shader** — see below.

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

Decoded by whatever the system already ships with, so nothing has to be
downloaded on either platform:

| Kind | Decoder | Plays |
| --- | --- | --- |
| Pictures | the engine, everywhere | png |
| Pictures | Windows Imaging Component | jpg, jpeg, bmp, webp, heic, tif, tiff |
| Videos | Media Foundation | mp4, mov, avi, wmv, m4v, gif and whatever else the system has a codec for |
| Pictures | ImageIO, macOS | jpg, jpeg, bmp, webp, heic, tif, tiff |
| Videos | AVFoundation, macOS | mp4, mov, m4v, gif and whatever else the system has a codec for |

The client itself keeps no list of formats. All it decides from the extension is
picture or video, and then it hands the file to the system: `png` goes to the
engine's own loader, the seven picture extensions above go straight to the still
decoder, and **everything else, `gif` included, is offered to the video decoder
first** and only tried as a picture if that refuses. A gif is handled that way
because it can be animated, and an animated one played as a video is the point.

So what actually plays is whatever the machine has a codec for, and the two
systems do not have the same ones: `avi` and `wmv` are Media Foundation's and
macOS opens neither, so a background that works on one machine can come up black
on the other. The format hint in the settings says whichever list applies.

Video shot on a phone is stored the way the camera sensor read it and carries a
matrix saying which way up it goes. All three decoders read that matrix and turn
the picture, so such a recording stands upright wherever it is played. Each one
asks a different question to get there: AVFoundation is asked for the track's
`preferredTransform`, Media Foundation for `MF_MT_VIDEO_ROTATION` on the source
type, and FFmpeg for the stream's `DISPLAYMATRIX` side data. The three do not
count the same way round, so each is converted to the one thing the copy out
needs: how far clockwise the frame has to be turned to stand the way it was shot.

The turn itself is free. Every frame is already copied out of the decoder pixel
by pixel to be swizzled into RGBA, so a quarter turn is only a different pair of
steps to walk the destination with, worked out once when the file is opened. The
FFmpeg path is the one exception: its scaler can only write rows the way they are
stored, so a turned frame is scaled into a buffer of its own and turned on the
way into the caller's image. A file that is not turned still goes straight from
the scaler into that image, as it always did.

`Width()` and `Height()` report the size the picture is seen at, after the turn,
so `cl_custom_background_fit` letterboxes a portrait recording the same way
everywhere. The frame size limit below is measured on that same turned size, for
the same reason, while the decoder is still asked to scale in the orientation it
works in.

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

A file the system decoder will not take is handed to FFmpeg rather than given up
on. That matters only where there is no system decoder at all, because the
FFmpeg shipped in `ddnet-libs` was compiled for **encoding only** and has no
decoders in it. To make that fallback do anything, put a full FFmpeg build of the
same major versions (`avcodec-61`, `avformat-61`, `avutil-59`, `swresample-5`,
`swscale-8`) beside the executable — inside `Contents/Frameworks` for the macOS
app, since that is the only place its `@rpath` looks.

The Media Foundation DLLs are delay loaded, so a Windows edition that ships
without them (the N editions) still starts and only fails to decode.

In game the map is drawn on top, so the background is only visible where the map
is see-through, for example with the entities overlay.

Above the theme list is the colour of the menus themselves, the panels and the
buttons and the bars. The game has always had that setting and has never had a
picker for it, so until now it could only be changed by typing `ui_color` into
the console with a packed number after it.

The menu's own moving background is a different thing: a small map, chosen by
theme, that the game has always had. Its themes live in `data/themes` and the
picker for them now sits at the bottom of the same settings page, rather than
three pages away under the general options. It is the game's own list, so a theme
chosen in one place is chosen in both.

### Sounds

A sound pack is a folder inside `sounds` holding files named after the game
sound sets, for example `hook_attach_ground.wav`, `hammer_hit.wav`,
`gun_fire.wav`, `player_spawn.wav`. Supported formats are **wav**, **opus** and
**wv** — plain WAV support was added to the engine for this, so files do not
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
album art, title, artist and the transport buttons. It slides in when the music
starts and slides out when it stops.

Where the track comes from depends on the system, and this is the one feature
whose reach genuinely differs between the two.

On **Windows** it is the media session, the same source as the volume flyout, so
every player that reports to the system works: Spotify, a browser tab, the system
player. Nothing has to be configured in the player itself.

On **macOS** there is no public equivalent, so two sources are tried in turn:

* **MediaRemote**, the private framework the media keys and Control Centre go
  through. It sees everything Windows does, browsers included, and it is the only
  one that hands over the cover art. It is opened by path at runtime rather than
  linked, so a system where it is missing or renamed still starts.
* **AppleScript** to Spotify and Music, reached only when MediaRemote answers
  nothing at all — never merely because nothing is playing, since addressing a
  player is what raises the automation prompt. A player that is not already
  running is never asked, so the client cannot start one by itself.

macOS 15.4 put MediaRemote's now playing information behind an entitlement only
Apple's own binaries carry, so from that release on the island sees Spotify and
Music and nothing else. **Sending commands still works**, which is why reading
and controlling are kept apart in the code: the buttons and the binds below keep
driving whatever owns the session even where the display has fallen back.

The first time the fallback talks to a player macOS asks whether to allow it, and
the answer is remembered under Privacy & Security → Automation. Refusing leaves
the island empty and breaks nothing else. A signed build additionally needs the
`com.apple.security.automation.apple-events` entitlement, which
`other/bundle/client/client.entitlements` carries — without it the hardened
runtime refuses the events before the user is ever asked, which looks like the
island being broken only on the signed copy.

Either way the query runs on its own thread and polls twice a second, so it costs
nothing on the render thread.

The pill holds the album art, the title, the artist, previous / play-pause /
next buttons and a progress bar along the bottom. The buttons can be clicked
wherever the mouse is a cursor, so in the menus; in game the mouse aims, and the
console commands below cover that.

There is no like button: neither source exposes anything for favouriting, only
play, pause and track skipping. Favouriting lives inside each player.

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

Those three keys are a poor fit for a Mac keyboard, which has no Pause key at all
and needs Fn for PgUp and PgDn on a laptop. Anything else works just as well:

```
bind f1 music_prev
bind f2 music_play_pause
bind f3 music_next
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

On the platforms that publish nothing the client can read, the island simply
never appears and the setting says so.

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
The shot reads two tunings, the way the game does: how far it reaches and how
soon the weapon has cooled down again come from the zone the tee is standing in,
while the bounce count and the bounce cost come from the zone the shot is fired
in.

The bounce delay is the one value that cannot be read per candidate. It has to
be known before the flight is predicted at all, because the horizon that
prediction runs to is itself measured in bounces, so it is sampled once at the
muzzle, from the zone the tee is standing in at the moment of the search. That
one number is then handed to everything downstream. It has to be one number: the
fire delays worth trying are enumerated on that bounce ladder and the trace then
walks it, so the two disagreeing would mean candidate shots tried at ticks the
beam never bounces on. They did disagree for a while. The trace read the tune
zone and the enumeration read the global tuning, and on any map that tuned
laser_bounce_delay in a zone the module searched a ladder it was not shooting on
and quietly found nothing at all.

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

**This needs OpenGL 3.3.** On Windows the client is on OpenGL whatever
`gfx_backend` says, because this build has no Vulkan backend compiled in, but
`gfx_gl_major` and `gfx_gl_minor` default to 1.1, which has no shader support at
all. Raising the two is what matters, and it takes a restart:

```
gfx_gl_major 3
gfx_gl_minor 3
```

**On macOS none of that is needed**: 3.3 is what the client already starts on
there. So the shader is compiled at every launch on a Mac, which is worth knowing
while editing it — a `tee.frag` that does not compile or does not link leaves the
tees drawn normally and puts the compiler's or linker's own message in the log,
rather than failing quietly.

On Vulkan the setting is ignored and tees render normally, because Vulkan needs
precompiled SPIR-V rather than GLSL source. That only comes up on Linux: the
Windows and macOS builds are configured without a Vulkan backend, and only
`-DVULKAN=ON` puts one back.

## Steam

**Config compatibility.** All settings use the `cl_custom_` prefix so they can
never collide with a variable that upstream DDNet might add later. The official
client stores config lines it does not know and writes them back out on save
(`CConfigManager::StoreUnknownCommand`), so these settings survive a round trip
through the Steam version untouched. Both clients share the same config file —
`%APPDATA%\DDNet\settings_ddnet.cfg` on Windows,
`~/Library/Application Support/DDNet/settings_ddnet.cfg` on macOS — so
sensitivity, binds and the rest stay in sync automatically.

The only upstream variable whose range changed is `ui_settings_page`, which now
allows the extra tab index; the official client clamps it back to its own range
and shows the Credits tab instead.

**Playtime.** Playtime cannot be added through code. Steam counts the runtime of
the process it launched itself under a given AppID, and no Steamworks call
exists to add hours. The only way to have this build count towards DDNet is to
put it where Steam launches DDNet from, i.e. replace `DDNet.exe` in
`steamapps\common\DDraceNetwork`.

On macOS Steam launches an app bundle rather than a bare executable, so the same
trick takes three steps instead of one:

```
~/Library/Application Support/Steam/steamapps/common/DDraceNetwork/DDNet.app/Contents/MacOS/DDNet
```

1. The binary has to go in under the name `DDNet`, because that is what the
   bundle's `CFBundleExecutable` names. Ours is called `Leviathan`.
2. The bundle keeps its own `Contents/Resources/data`, which is DDNet's and has
   no `shader/tee.vert`, `shader/tee.frag` or `leviathan_logo.png` in it. Without
   them `cl_custom_tee_shader` does nothing and says so in the log, and the start
   menu falls back to the name in writing. The log line is

   ```
   No usable shader/tee.vert and shader/tee.frag, the custom tee shader stays off
   ```

   which is also what a shader that fails to compile or link says, so it is not
   by itself proof of a missing file. Copy the three files in, or put the two
   shaders in `~/Library/Application Support/DDNet/shader/`, which takes
   precedence over `data` anyway.
3. Editing a bundle invalidates its signature, and on Apple Silicon an unsigned
   binary will not run at all, so it has to be signed again afterwards:
   `codesign --force --deep --sign - .../DDNet.app`.

## Antiping

Already part of upstream DDNet, nothing was added: `cl_antiping` plus
`cl_antiping_players`, `cl_antiping_grenade`, `cl_antiping_weapons`,
`cl_antiping_smooth`, `cl_antiping_gunfire`, `cl_antiping_preinput`,
`cl_antiping_limit` and `cl_antiping_percent`.

