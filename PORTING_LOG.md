# Porting log: Leviathan on macOS (Apple Silicon)

Every problem met while bringing the Windows client to macOS arm64, with its
cause, the fix, and how the fix was checked. Read this before starting a new
stage, so nothing here is solved a second time.

The port lives in the same tree as the Windows client. Shared code is shared;
the platform-specific parts sit behind `CONF_PLATFORM_MACOS` / `CONF_FAMILY_UNIX`
or in their own files (`src/macos/`, `*_mac.mm`, the unix half of
`discord_ipc.cpp`). The Windows client is built from the same commit and is not
changed by any of this.

Builds run on GitHub Actions (`.github/workflows/macos-app.yml`, runner
`macos-26`, which is Apple Silicon). There is no Mac in the development
environment, so "checked" below means: the CI build passed and, where stated, a
headless client ran to a clean shutdown. What could not be checked without a
window server is listed at the end, not hidden.

---

## Error 1

**Error:**
The packaged app opened to a grey window and hung. The log on the Mac read:
`no data directory found` … `$CURRENTDIR ('/')`.

**Cause:**
`src/macos/client.mm` decided whether it was running inside an app bundle with
`[bundlePath isEqualToString:@"DDNet.app"]`. The bundle is `Leviathan.app`, so
the check failed, the working directory was never changed into
`Contents/Resources`, and the client started with `/` as its current directory
and found no `data/`.

**Fix:**
The check became `[bundlePath hasSuffix:@".app"]`: any bundle counts, whatever
it is called.

**Check:**
CI build green; release `v20.1-macos-3` started on the user's Mac and reached the
game.

---

## Error 2

**Error:**
Nothing to read when the app failed: macOS users cannot easily open a terminal
to see the client's output, and a client that dies at startup leaves nothing
behind.

**Cause:**
`logfile` is empty by default, so the log only goes to stdout, which a bundled
app has nowhere to show.

**Fix:**
`client.cpp`: on macOS, when `logfile` is empty, it defaults to
`~/Desktop/Leviathan.log`.

**Check:**
CI build green; the log file was what diagnosed Error 1.

---

## Error 3

**Error:**
Rich presence buttons (the Telegram link on the Discord profile) had no
implementation on macOS.

**Cause:**
Windows reaches Discord through a named pipe. The unix family reaches it through
a unix domain socket in `$XDG_RUNTIME_DIR`, `$TMPDIR` or `/tmp`, and the address
structure carries the path in a 104-byte field on macOS, which the long per-user
`$TMPDIR` can exceed.

**Fix:**
`discord_ipc.cpp` has one transport per family behind the same frame protocol.
The unix one tries each directory, skips a path that would not fit the field,
sets `SO_NOSIGPIPE` so Discord quitting mid-game does not kill the client, and
retries short writes.

**Check:**
Compiles in CI. Not exercised end to end: the runner has no Discord client.

---

## Error 4

**Error:**
The `.dmg` built in CI would have failed to launch on any Mac but the runner.

**Cause:**
CMake resolved freetype, png, opus and ogg from Homebrew under `/opt/homebrew`
because `ddnet-libs` is only reached through `PATHS`, the last place
`find_library` looks. Those dylibs carry absolute Homebrew install names, the
copies placed in `Contents/Frameworks` are referenced by nothing, and a Mac
without Homebrew has nothing at those paths.

**Fix:**
`-DPREFER_BUNDLED_LIBS=ON` in the workflow, so `ddnet-libs` wins for everything
it carries.

**Check:**
`v20.1-macos-3` ran on a Mac that had never had Homebrew.

---

## Error 5

**Error:**
Risk, not yet a failure: thirty-one commits of new features landed after the last
macOS build, all compiled only with MSVC.

**Cause:**
MSVC's standard headers include one another generously; libc++ on macOS does
not. `std::size` needs `<iterator>`, `std::clamp` / `std::max({…})` need
`<algorithm>`, `std::sin` / `std::fmod` need `<cmath>`, `getenv` needs
`<cstdlib>`. Code that names these without including them compiles on Windows
and fails on macOS.

**Fix:**
Explicit includes added to `particles3d.cpp`, `hud.cpp`, `items.cpp`,
`gameclient.cpp`, `text.cpp`, `menus_settings_leviathan.cpp`, `client.cpp`.

**Check:**
Windows build still green (`build.bat`). macOS CI: see the entries that follow.

---

## Not checkable here

The CI runner has no window server, so these were not exercised on macOS and
are stated as untested rather than as working:

- window creation, fullscreen, windowed mode, resolution changes, Retina scaling;
- keyboard, mouse, text input, clipboard;
- sound output;
- the OpenGL rendering itself (only the headless backend runs in CI);
- connecting to a server from the packaged app.

What CI does check on every run: the full client links against the bundled
frameworks, the Objective-C++ files compile, and a headless client starts,
executes commands, saves its config and shuts down cleanly, in both Release and
Debug.
