# Leviathan

A DDNet 20.1 client with its own additions on top of the game. Built for
**Windows** and **macOS (Apple Silicon)** from one source tree: the two clients
are the same program.

- Telegram: <https://t.me/leviathanddnet>
- Windows and macOS builds: [Releases](https://github.com/sdenar228-alt/Leviathan-/releases)
- The macOS build's own repository: <https://github.com/sdenar228-alt/Leviathan-macos>

## What is added

Everything is configured in the **Leviathan** tab of the settings (or from the
console, F1).

- **Unfreeze shot** — the client works out how to unfreeze you with your own
  laser: draws the plan, or fires it on a key.
- **Tee trail** — a ribbon behind the tee, with colour, width, length and fade.
- **3D particles** — wireframe cubes, hearts, circles, hexagons and triangles
  drifting behind the map, with thickness and glow.
- **Fancy weapons** — the crystal laser and the sandy shotgun, sparks along the beam.
- **Hats** — a crown, horns, a halo, and any png of your own from the `hats` folder.
- **Focus mode** — one key strips the screen down to the game.
- **Gradient text** — in the game and in the menus, with speed, colour, spread
  and brightness; black shimmers black and white.
- **Client plate** — logo, name and the computer's clock above the timer.
- **Auto reply** — to muted players and while you are away, in your own words.
- **Leviathan players** — the logo by the name and in the scoreboard of everybody
  on this client.
- **Friends and war** — green and red dots by the name, `!war name` from the chat.
- **Music island** — what the system is playing, with cover and buttons.
- **Background** — a picture or a video behind the game and the menus; the
  menu's own colour and theme.
- **Tile colours, custom crosshair, hook colour, tee outline, avatar instead of
  the body, tee shader, sound packs, weapon models from other packs.**
- **Discord presence** with a Telegram button.
- **RU / EN language switch** right in the main menu.

Every feature, and how it works: [LEVIATHAN.en.md](LEVIATHAN.en.md)
(Russian: [LEVIATHAN.md](LEVIATHAN.md)).

## Building

**Windows** (Visual Studio with C++, CMake, Ninja):

```
build.bat
```

The result is `build\Leviathan.exe` with the `data` folder beside it.

**macOS**: see [docs/README.macos.en.md](docs/README.macos.en.md) for the
requirements, the dependencies, the CMake commands and the `.dmg` packaging. The
port is described in [PORTING_LOG.md](PORTING_LOG.md) (Russian).

`ddnet-libs` is not part of the repository; clone it beside the tree:

```
git clone --depth 1 https://github.com/ddnet/ddnet-libs
```

## Installing a release

- **Windows:** unpack the archive from Releases and run `Leviathan.exe`.
- **macOS:** open the `.dmg`, drag `Leviathan.app` into Applications, and let it
  through once under Privacy & Security → Open Anyway.

Settings live where stock DDNet keeps them (`%APPDATA%\DDNet` on Windows,
`~/Library/Application Support/DDNet` on macOS), so a stock config carries over.

## License

DDNet's license, see [license.txt](license.txt). DDNet's original README is kept
as [docs/README.ddnet.md](docs/README.ddnet.md).

---

Русская версия: [README.md](README.md)
