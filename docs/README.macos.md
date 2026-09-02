# Leviathan for macOS (Apple Silicon)

Leviathan is a DDNet 20.1 client with its own set of additions on top of the
game: an automatic unfreeze shot, tee trails, 3D wireframe particles drifting
behind the map, dressed-up laser beams, hats, a focus mode that strips the
screen down to the game, gradient text, a music island showing what the system
is playing, a friends-and-enemies system with coloured dots, auto replies, custom
backgrounds (pictures and video), tile colours, a custom crosshair, hook colours,
Discord rich presence with a Telegram button, and a settings page that gathers
all of it under one tab.

This is the **macOS build for Apple Silicon (arm64)**. It is built from the same
source tree as the Windows client, commit for commit: everything that is not
about the operating system is shared, and what is about it lives in its own
files. The two clients look and behave the same because they are the same
program.

## Requirements

- A Mac with Apple Silicon (M1 or later). Intel Macs are not built for.
- macOS 11 (Big Sur) or later: that is the first macOS that runs on Apple
  Silicon at all. The binary's own deployment target is 10.15.
- Nothing else to install: the app bundle carries every library it needs.

## Installing the release

1. Download `Leviathan-<version>-macos.dmg` from the Releases page.
2. Open the image and drag `Leviathan.app` into `Applications`.
3. The app is not signed or notarized, so macOS refuses it the first time. Two
   ways past that:
   - open **System Settings → Privacy & Security**, scroll to the note about
     Leviathan being blocked, and choose **Open Anyway**; or
   - in Terminal: `xattr -dr com.apple.quarantine /Applications/Leviathan.app`
4. Start it from `Applications`.

If it does not start, the client writes a log to `~/Desktop/Leviathan.log`
by default on macOS. That file is the first thing to look at, and the first thing
to send when asking for help.

## Building from source

Everything below happens in Terminal.

```bash
# Xcode command line tools, if they are not there yet
xcode-select --install

# Build tools and the libraries CMake will look for
brew install cmake ninja pkg-config sdl2 ffmpeg
brew upgrade freetype
python3 -m pip install --break-system-packages dmgbuild pyobjc-framework-Quartz

# The prebuilt libraries the bundle ships with (not a submodule)
git clone https://github.com/sdenar228-alt/Leviathan-macos.git
cd Leviathan-macos
git clone --depth 1 https://github.com/ddnet/ddnet-libs ddnet-libs

# Configure and build the client
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPREFER_BUNDLED_LIBS=ON \
  -DCLIENT_EXECUTABLE=Leviathan \
  -DDISCORD=ON
cmake --build build --target game-client

# Optional: the .app bundle and the disk image
cmake --build build --target package_dmg
```

`PREFER_BUNDLED_LIBS=ON` matters. Without it CMake picks freetype, png, opus
and ogg from Homebrew, whose dylibs carry absolute `/opt/homebrew` paths, and the
resulting app only runs on a machine that has Homebrew at the same paths.

For a Debug build, use `-DCMAKE_BUILD_TYPE=Debug` and a different build
directory.

## Running from the build directory

```bash
cd build
./Leviathan
```

The client finds its `data/` directory beside the executable. Settings are
written to `~/Library/Application Support/DDNet/`, the same place the stock
DDNet client uses, so a stock config carries over.

## What is different on macOS

- **Music island** reads what the system says is playing through the Now Playing
  service. From macOS 15.4 on, only Spotify and Music can be read, and the first
  time the system asks whether to allow it.
- **Video backgrounds** decode through AVFoundation; pictures (png, jpg, bmp,
  webp, heic) and videos (mp4, mov, m4v) all work.
- **Discord presence** talks to the Discord app through its unix socket rather
  than a named pipe. Same buttons, same picture.
- **The log** goes to `~/Desktop/Leviathan.log` unless `logfile` is set.
- **Graphics** use OpenGL 3.3. The Vulkan backend is not built on macOS.

## Known limitations

- The app is unsigned. Every fresh download has to be let through Gatekeeper
  once (see Installing).
- Apple Silicon only. There is no Intel build and no universal binary.
- The custom tee shader (`shader/tee.frag`) needs the OpenGL 3.3 backend, which
  is what the client starts on by default.

## How the port was done

`PORTING_LOG.md` lists every problem met on the way to macOS, its cause, the fix
and how the fix was checked. It also states plainly which parts have only been
verified by the automated build and not by a person at a Mac.

## Continuous builds

`.github/workflows/macos-app.yml` builds the client on Apple Silicon runners in
Release and Debug, runs a headless client through startup and shutdown, and
packages the Release build into the disk image published on the Releases page.
