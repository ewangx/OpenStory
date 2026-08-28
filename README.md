

## Screenshots

### Status Bar
![Status Bar](docs/images/statusbar.png)

### Emoji Support & Minimap
![Emoji & Minimap](docs/images/emoji-minimap.png)

### Quest UI & NPC Quest Indicators
![Quest UI](docs/images/quest-ui.png)

## Features

- Full v83 client connecting to Cosmic servers
- NX file-based assets, OpenGL rendering, GLFW windowing
- Login flow, character select, world/channel select, PIC entry
- Combat, skills, buffs, inventory, quests, NPCs, chat
- Storage, buddy list, skill macros, gamepad support
- Distance-based (spatial) sound for world events
- Fullscreen, UI scaling, drag-and-drop windows

### Text & input

- Fonts are **compiled into the binary** (Roboto + Noto Sans Hebrew) — the client needs no
  font files at runtime and does not depend on `C:/Windows/Fonts`, so glyphs never
  silently vanish on another machine.
- **Right-to-left text** (Hebrew): Unicode bidi reordering, right-aligned wrapping, and a
  caret that tracks the visual position rather than the byte offset.
- **Unicode input** via the OS keyboard layout, with character-safe editing — backspace,
  delete and the arrow keys move whole characters, not bytes.
- Inline icons in chat (`#v/#i/#q/#s/#f/#e`) and an emoticon picker on the chat bar.

Non-ASCII text requires the server to use UTF-8. On Cosmic that is
`CHARSET: UTF-8` in `config.yaml`; note its `CharsetConstants` whitelist must contain
UTF-8 or it silently falls back to US-ASCII and mangles everything to `?`.

### Custom / experimental

Some features are client-side additions not supported by a stock Cosmic server:

| Feature | Status | Notes |
|---------|--------|-------|
| Event System | WIP | Custom `EVENT_INFO` / `REQUEST_EVENT_INFO` packets; needs a server-side handler |
| HP/MP Warning | Working | Client-only |
| Graphics/Effects Quality | Working | Client-only |

## Building

### Requirements
- Visual Studio 2022+, Windows SDK, CMake 3.15+
- Dependencies: GLFW, GLEW, FreeType, Bass audio, NoLifeNx, Asio

### Build
```bash
cmake -S . -B build
cmake --build build --config Debug --target OpenStory
# Output: wz/OpenStory.exe
```

Place v83 NX files in the `wz/` directory.

### macOS

The initial macOS build uses a null audio backend. Install the native dependencies and build the app bundle:

```bash
brew install freetype glfw glew asio
cmake -S . -B build/macos \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$(brew --prefix)" \
  -DOPENSTORY_AUDIO=OFF
cmake --build build/macos --parallel
```

Launch from a logged-in Terminal session and point the client at the validated NX directory:

```bash
open -n \
  --env OPENSTORY_ASSET_DIR=/absolute/path/to/nx \
  wz/OpenStory.app
```

The environment variable changes the process working directory before settings, networking, or assets are initialized. This avoids copying the NX set into the app bundle. Audio remains intentionally disabled until a portable backend is added.

When the primary `UI.nx` comes from a newer MapleStory version, place a Cosmic-v83 UI archive at `UI.v83.nx`. OpenStory uses this optional overlay for legacy login screens that are absent from newer UI data, while all other UI nodes continue to come from `UI.nx`.

To run against a sibling Cosmic checkout, start the local server before launching the client:

```bash
docker compose -f ../Cosmic/docker-compose.yml up --build -d
docker compose -f ../Cosmic/docker-compose.yml logs -f maplestory
```

Cosmic is ready when its log says `Cosmic is now online`. The client defaults to `127.0.0.1:8484`; Cosmic exposes channels on ports `7575` through `7577`. Stop the stack with:

```bash
docker compose -f ../Cosmic/docker-compose.yml down
```

Fonts are compiled in via the generated `src/Graphics/EmbeddedFonts.{h,cpp}`, which are
committed — no extra build step.

## Configuration

Edit `Configuration.h` for defaults. A `Settings` file is generated after the first run.

First run: enable **Save Login** on the login screen, close the client, then edit the
generated `Settings` file to set `VSync = false` and `Fullscreen = true`.

## Quest Helper

Track up to 5 quests at once with live progress updates.

- **Add a quest**: Open the Quest Log (Q), go to In-Progress, then drag a quest into the Quest Helper (or double-click an opened quest)
- **Remove a quest**: Click the X button next to the quest name
- **Reorder**: Drag a quest name up or down within the Quest Helper
- **Collapse/Expand**: Click a quest name to toggle its requirements
- **Auto-track**: Click the AUTO button to fill the helper with your active quests

## Credits

- **Daniel Allendorf & Ryan Payton** — Original [HeavenClient](https://github.com/HeavenClient/HeavenClient)
- **rdiol12** — v83 Cosmic compatibility, UI systems, packet handlers

## License

GNU Affero General Public License v3. See [LICENSE](LICENSE).
