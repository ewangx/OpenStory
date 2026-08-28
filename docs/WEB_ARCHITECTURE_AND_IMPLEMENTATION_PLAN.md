# OpenStory Web Architecture and Implementation Plan

Status: Proposed  
Target: OpenStory native and WebAssembly clients from one gameplay codebase  
Last updated: 2026-08-27

## Executive Summary

OpenStory should become the authoritative implementation for gameplay, protocol, data, and in-game UI on both native and web platforms. The web client should not be maintained as a separate fork of an older JourneyClient or LibreMaple snapshot.

The recommended implementation is an incremental extraction of platform boundaries from the existing OpenStory source. Native Windows behavior remains supported while browser implementations are added for transport, assets, audio, input, persistence, timing, and rendering integration.

The target architecture has these properties:

- One shared C++17 gameplay and UI implementation.
- Native and Emscripten builds from the same repository and commit.
- A nonblocking client lifecycle that does not require Emscripten Asyncify.
- A WebAssembly game worker with an `OffscreenCanvas` where supported.
- WebGL2 as the first browser renderer; WebGPU is a future backend, not a prerequisite.
- Binary WebSocket gameplay traffic through a restricted TCP gateway.
- Versioned, content-addressed assets delivered over HTTP and cached locally.
- Packet fixtures, session replays, browser smoke tests, and visual regression tests.
- Server-authoritative inventory, economy, ownership, and combat outcomes.

This is an architectural migration, not a rewrite. Existing OpenStory behavior should move behind narrow interfaces before internal systems are redesigned.

## Goals

- Run the current OpenStory feature set in modern desktop browsers.
- Preserve the native Windows client throughout the migration.
- Eliminate repeated feature porting between native and web repositories.
- Keep protocol framing, cryptography, packet parsing, gameplay, and Maple UI in shared C++.
- Make browser-specific behavior explicit and independently testable.
- Support a pinned Cosmic v83 server revision with an authoritative opcode map.
- Make asset versions reproducible through a hashed manifest.
- Provide a secure deployment model suitable for a public web client.
- Add enough automated coverage to migrate packet-heavy features safely.

## Non-Goals

- Rewriting the client in TypeScript.
- Replacing the existing gameplay model with an ECS.
- Rebuilding all UI with HTML and CSS.
- Requiring WebGPU for the first release.
- Supporting arbitrary user-selected TCP destinations through the gateway.
- Distributing Nexon-owned assets without authorization.
- Treating custom OpenStory protocols as stock Cosmic v83 behavior.
- Porting the Windows launcher, native crash dumps, or hardware identifiers to the browser.
- Matching mobile browser behavior in the first desktop-web milestone.

## Current Constraints

The existing source has several platform assumptions that must be separated without changing gameplay behavior:

- `src/MapleStory.cpp` performs synchronous initialization and owns a blocking loop with `sleep_for`.
- `src/Net/Session.h` chooses Asio or Winsock at compile time and directly owns the socket.
- Packet handlers commonly mutate singleton gameplay and UI state.
- `src/Audio/Audio.cpp` is implemented with BASS, although the public audio behavior can be expressed independently.
- `src/IO/Window.*` combines windowing, input, clipboard, screenshots, display settings, and frame presentation.
- `src/Graphics/GraphicsGL.*` assumes desktop OpenGL types and capabilities.
- `src/Util/NxFiles.*` assumes locally available NX archives.
- Settings, packet warnings, screenshots, and custom art use native filesystem paths.
- Some font fallbacks use Windows font paths.
- The CMake target discovers all source files recursively, which makes platform ownership implicit.

The migration should first expose these assumptions through interfaces. It should not begin by rewriting gameplay, physics, or every packet handler.

## Architecture Principles

### One Authoritative Client

OpenStory is the source of truth for shared behavior. Native and web targets must compile the same protocol, character, gameplay, data, and Maple UI source.

A change to a quest handler, mob update, social window, or packet layout should be implemented once and verified on both targets.

### Ports and Adapters

Shared code depends on small platform contracts. Native and web code implement those contracts.

Platform code must not contain game rules. Shared code must not include Winsock, browser JavaScript, BASS, IndexedDB, or OS-specific headers.

### Nonblocking Lifecycle

Connection, asset loading, and startup are represented as state machines. The browser target must not emulate blocking calls with Asyncify.

The main loop advances whichever startup or game state is active:

```text
Boot
  -> LoadConfiguration
  -> LoadAssetManifest
  -> InitializeRenderer
  -> InitializeAudio
  -> ConnectGateway
  -> Handshake
  -> Login
  -> Game
  -> Reconnecting or Stopped
```

### Server Authority

WebAssembly and JavaScript are inspectable and modifiable by users. The server must validate all inventory, currency, movement, ownership, reward, and battle operations. Client-side checks are usability features, not security controls.

### Incremental Compatibility

The native client is the behavioral reference during migration. Architectural cleanup is acceptable only when protected by packet fixtures, replay tests, or explicit native/web comparison.

## Target Repository Structure

The exact move sequence can be incremental, but ownership should converge on this structure:

```text
client/
  app/                 lifecycle, startup states, main client coordinator
  core/                errors, time, common types, shared utilities
  protocol/            framing, crypto, packet readers/writers, opcodes
  character/           player state, inventory, skills, buffs
  gameplay/            maps, physics, combat, entities, stage
  data/                typed views over game assets
  ui/                  in-game canvas UI and input routing
  render/              render commands and renderer contract
  platform/
    native/            GLFW, OpenGL, BASS, sockets, native filesystem
    web/               Emscripten, WebGL2, WebAudio bridge, web storage
web/
  shell/               HTML, CSS, TypeScript bootstrap and diagnostics
  gateway/             authenticated WebSocket-to-Cosmic gateway
tools/
  content-packer/      asset validation, chunking, hashing, manifests
tests/
  packets/             inbound and outbound packet fixtures
  replays/             captured deterministic sessions
  screenshots/         visual baselines
  browser/             Playwright end-to-end tests
```

Moving all existing files immediately would produce noisy history. New boundaries should be introduced in the current layout first, followed by mechanical moves after both targets build.

## Core Platform Contracts

The names below are illustrative. Keep interfaces narrow and driven by actual call sites.

### Transport

```cpp
class Transport {
public:
    virtual ~Transport() = default;
    virtual void connect(const Endpoint& endpoint) = 0;
    virtual void send(std::span<const std::byte> bytes) = 0;
    virtual void close() = 0;
    virtual TransportState state() const = 0;
    virtual void poll(TransportEvents& events) = 0;
};
```

Native implementation:

- Direct TCP through Winsock initially.
- Asio can replace Winsock later without affecting `Session`.

Web implementation:

- Binary WebSocket to the game gateway.
- No client-selected raw upstream host.
- Reconnection is coordinated by the application lifecycle, not hidden in the adapter.

`Session` continues to own Maple framing, cryptography, handshake state, packet assembly, and packet dispatch. It no longer owns an OS socket.

### Asset Store

```cpp
class AssetStore {
public:
    virtual ~AssetStore() = default;
    virtual AssetRequest request(const AssetKey& key) = 0;
    virtual AssetState state(AssetRequest request) const = 0;
    virtual AssetView view(AssetRequest request) const = 0;
};
```

Native implementation:

- Reads validated local content packs or NX files.

Web implementation:

- Fetches immutable chunks over HTTP.
- Verifies content hashes.
- Uses Cache Storage or OPFS for persistence.
- Exposes asynchronous completion without blocking the WASM runtime.

The existing `nl::node` access pattern can remain during the first milestone. The adapter may provide the backing bytes expected by NoLifeNx while the longer-term content format is evaluated.

### Audio Output

The shared audio API should express intent rather than BASS handles:

```cpp
class AudioOutput {
public:
    virtual ~AudioOutput() = default;
    virtual SoundHandle loadSample(AudioData data) = 0;
    virtual void play(SoundHandle sound, const PlaybackParams& params) = 0;
    virtual void playMusic(AudioData data, bool loop) = 0;
    virtual void setMusicVolume(float value) = 0;
    virtual void setSoundVolume(float value) = 0;
};
```

`PlaybackParams` carries volume and pan so OpenStory's spatial-audio calculation remains shared.

Native implementation uses BASS. Web implementation uses Web Audio and begins only after a user gesture unlocks the `AudioContext`.

### Clock and Frame Driver

Shared code receives elapsed time and never sleeps directly.

Native implementation uses the existing fixed timestep inside a GLFW frame driver. Web implementation uses `emscripten_set_main_loop` or `requestAnimationFrame` through a small bridge.

Both implementations must apply the existing catch-up clamp. The web implementation additionally handles visibility changes, suspended tabs, and clock discontinuities.

### Input and Window Services

Separate these responsibilities from the current `Window` class:

- Logical pointer and keyboard events.
- Unicode character input.
- Gamepad state.
- Clipboard access.
- Fullscreen requests.
- Logical and physical viewport sizes.
- Frame begin/present.
- Screenshot requests.

Native input remains GLFW-based. Web input enters through browser events or Emscripten GLFW shims and is normalized into the same logical coordinate system.

### Persistence and Diagnostics

Persistence stores non-sensitive preferences only. Passwords must not be persisted by the web target.

Native diagnostics can write local logs and crash reports. Web diagnostics should use structured console output and an optional telemetry sink. Packet failures must retain opcode and parser context instead of being silently discarded.

## Runtime Data Flow

Inbound flow:

```text
WSS gateway
  -> Transport bytes
  -> Maple packet framing
  -> Cryptography
  -> Opcode dispatch
  -> Typed packet handler
  -> Domain state update
  -> UI/gameplay reads updated state
  -> Render command submission
```

Outbound flow:

```text
Browser/native input
  -> Shared input event
  -> UI or gameplay command
  -> OutPacket serialization
  -> Cryptography and framing
  -> Transport send
```

The first release may preserve direct state mutation inside existing handlers. New and substantially changed handlers should move toward typed domain operations so they can be tested without constructing the entire UI singleton graph.

## Browser Execution Model

### Main Thread

The browser main thread owns:

- The HTML shell and loading/error screens.
- The initial user gesture required for audio.
- Browser navigation and external links.
- Accessibility and diagnostics overlays.
- Forwarding input when direct worker input is unavailable.

### Game Worker

The game worker owns:

- The WebAssembly instance.
- The fixed-step game loop.
- Packet framing and game state.
- Asset decoding.
- WebSocket communication with the gateway.
- `OffscreenCanvas` rendering where supported.

A main-thread canvas fallback may be retained for browsers without the required worker capabilities. Desktop Chrome and Firefox are the first supported targets; Safari support follows after core parity.

### Memory

- Establish an initial and maximum WASM memory budget from measured gameplay sessions.
- Avoid unconstrained memory growth because growth can pause execution and invalidate JavaScript views.
- Track decoded asset memory separately from GPU texture memory.
- Add explicit map-transition and atlas memory tests.
- Add deterministic eviction for assets that can be reloaded.

Threads and `SharedArrayBuffer` are deferred until profiling proves they are needed because they impose cross-origin isolation requirements.

## Rendering Strategy

### First Release: WebGL2

WebGL2 is the compatibility target. The renderer should be made valid for both desktop OpenGL and OpenGL ES 3/WebGL2:

- Replace `GL_QUADS` with indexed triangles.
- Use WebGL2-compatible texture formats and uploads.
- Replace implicit desktop GL behavior with explicit state.
- Use GLSL ES 3.00-compatible web shaders.
- Keep logical game coordinates independent from canvas pixels.
- Preserve nearest-neighbor sampling for pixel art.
- Handle device-pixel ratio without changing UI layout.
- Handle WebGL context loss and restoration.
- Avoid synchronous readbacks in the normal frame path.
- Add GPU and atlas memory metrics.

The existing sprite and Maple UI classes should remain. A DOM rewrite would create a second UI implementation and make visual parity harder.

### Future: WebGPU

WebGPU is considered only after WebGL2 parity and profiling. The render contract should allow a future WebGPU backend, but no initial milestone depends on it.

## Asset Architecture

### Content Pipeline

The web client should consume a generated, versioned asset set:

```text
Authorized WZ/NX input
  -> validate expected files and nodes
  -> normalize metadata
  -> split into load-oriented chunks
  -> compress
  -> hash
  -> emit manifest
  -> publish immutable files
```

Suggested chunk categories:

- Boot, login, and common UI.
- Character and equipment.
- Common effects and skills.
- Maps grouped by region.
- Mobs and NPCs grouped by use.
- Items, strings, and quest data.
- Music and sound grouped for streaming.

The initial proof of concept may expose the existing NX files through HTTP range requests. Production should use explicit immutable chunks when measurements show that it improves startup, caching, or memory use.

### Manifest

The manifest must contain:

- Schema version.
- Asset-set identifier.
- Compatible client commit or version range.
- Compatible server profile.
- Source asset expectations, including UI version.
- Chunk URLs, hashes, and sizes.
- Compression and content type.
- Required and optional chunks.

Each deployment references an immutable manifest. Publishing new content produces a new manifest instead of mutating cached files.

### Browser Cache

- Use ordinary HTTPS for delivery and CDN caching.
- Use content hashes as immutable cache keys.
- Use Cache Storage for fetched responses.
- Use OPFS for large random-access content if browser support and measurements justify it.
- Keep IndexedDB for metadata where necessary, not as an unversioned opaque asset cache.
- Provide a cache reset and diagnostics screen.

## Network Gateway

Browsers cannot connect directly to Cosmic's TCP socket, so a gateway is required.

```text
Browser -- WSS --> Game Gateway -- TCP --> Pinned Cosmic Server
```

The gateway should remain protocol-transparent except for connection policy and operational controls.

Required controls:

- HTTPS and WSS only in production.
- Configured server identifiers mapped to an upstream allowlist.
- No arbitrary hostname or port supplied by the browser.
- Origin validation.
- Authentication or signed short-lived connection tokens when appropriate.
- Connection, bandwidth, message-size, and idle limits.
- Handshake timeout.
- Per-IP and per-account abuse controls.
- Structured logs, connection metrics, and error categories.
- Graceful shutdown and connection draining.

WebTransport and WebRTC are not initial requirements. MapleStory expects an ordered TCP stream, which maps naturally to one binary WebSocket.

## Protocol and Server Profiles

The first release supports one pinned Cosmic revision. Record the following in a server profile:

- Protocol version and locale.
- Send and receive opcode tables.
- Handshake behavior.
- Character creation and PIC behavior.
- Enabled stock systems.
- Enabled custom OpenStory extensions.
- Required asset manifest.

OpenStory custom systems must be capability-gated:

- Event System.
- Bot inventory and bot movement.
- Monster Life and Monster Battle.
- Procedural equipment formats.
- Custom jobs or server-specific fields.

The client must not send custom opcodes unless the selected server profile declares support.

## Security Requirements

- Never persist account passwords in local storage, IndexedDB, OPFS, cookies, or configuration files.
- Do not expose a general-purpose TCP proxy.
- Validate all gateway destinations server-side.
- Treat all client state and commands as untrusted on the game server.
- Enforce maximum packet and asset sizes before allocation.
- Validate asset hashes before use.
- Escape or constrain browser-shell text rendered through HTML.
- Use a restrictive Content Security Policy.
- Pin production dependencies and scan containers in CI.
- Do not collect hardware identifiers or volume serial numbers in the browser.
- Redact credentials and session tokens from logs and telemetry.

## Testing Strategy

### Native Build Safety Net

Every shared-code change must continue to build the Windows client. Existing Windows CI remains required.

### Packet Fixtures

Add fixtures for:

- Handshake and encrypted framing.
- Login, world, channel, and character selection.
- Map entry and channel transfer.
- Inventory and stat updates.
- NPC dialogue, shops, and storage.
- Quests and quest progress.
- Party, buddy, guild, alliance, and family flows.
- Pets, mounts, summons, and field objects.
- Cash shop and MTS when enabled.

Each inbound fixture asserts the decoded values and resulting domain state. Each outbound fixture asserts exact bytes after serialization and before encryption, plus framing tests where useful.

### Session Replays

Capture sanitized sessions against the pinned Cosmic server and replay them without a live connection. Replays should be deterministic enough to compare native and WASM state checkpoints.

### Visual Regression

Capture deterministic screenshots for:

- Login and character selection.
- Representative maps.
- Inventory, stats, skills, and quest log.
- Shops, storage, trade, and social windows.
- Unicode and RTL text.
- Multiple logical resolutions and device-pixel ratios.

### Browser End-to-End Tests

Use Playwright for:

- Boot and asset download.
- Login through map entry.
- Map transition and reconnect.
- NPC dialogue and shop transaction.
- Quest acceptance and progress.
- Storage and representative social actions.
- Cache reuse and cache upgrade.
- Visibility changes and background-tab recovery.
- WebGL context restoration where testable.

## Build and CI Strategy

Replace recursive source globbing with explicit target ownership as platform boundaries emerge:

```text
openstory_core
openstory_native_platform
openstory_web_platform
OpenStory
OpenStoryWeb
```

CI should eventually include:

- Windows native Release build.
- Linux native compilation as a portability check.
- Emscripten Release build.
- C++ unit and packet fixture tests.
- Browser smoke tests in Chrome and Firefox.
- Asset-manifest validation with non-proprietary fixture data.
- Formatting or lint checks introduced only after the initial port is stable.
- Dependency and container security scans for public deployment.

Production web artifacts must be immutable and versioned. Do not use a rolling release as the only rollback mechanism.

## Implementation Plan

Estimates are cumulative person-effort for engineers familiar with C++17, Emscripten, WebGL, Cosmic, and NX data. Calendar duration depends on team size and parallelism.

### Phase 0: Reproducible Baseline

Estimate: 3-5 engineer-weeks

Deliverables:

- Pin the Cosmic server revision and opcode tables.
- Define stock, server-dependent, custom, and WIP feature inventories.
- Produce a known-good asset manifest for development.
- Capture representative packet fixtures and sanitized sessions.
- Add a minimal native test executable.
- Record native screenshots and gameplay checkpoints.
- Document browser support targets and performance budgets.

Exit criteria:

- A new developer can build native OpenStory and run the pinned test environment.
- Core packet fixtures run without a live server.
- Asset inputs are reproducible and their required versions are explicit.

### Phase 1: Platform Seams

Estimate: 6-10 engineer-weeks

Deliverables:

- Introduce transport, clock/frame, persistence, diagnostics, and external-link contracts.
- Adapt the native client to those contracts without intended behavior changes.
- Replace blocking startup with an application lifecycle state machine.
- Separate native socket ownership from `Session`.
- Separate filesystem logging and settings from shared configuration.
- Split CMake targets by shared and native ownership.

Exit criteria:

- Native OpenStory remains playable.
- Shared targets contain no Winsock, Windows, BASS, or launcher dependencies.
- Startup and reconnect behavior can advance without blocking calls.

### Phase 2: Minimal Web Client

Estimate: 8-12 engineer-weeks

Deliverables:

- Emscripten target and TypeScript/HTML shell.
- Binary WebSocket transport adapter.
- Development-only restricted TCP gateway.
- Web frame driver without Asyncify.
- WebGL2-compatible renderer path.
- Browser keyboard, pointer, Unicode, clipboard, and fullscreen integration.
- Initial HTTP asset loading and persistent cache.
- Web Audio adapter with user-gesture initialization.

Exit criteria:

- The shared OpenStory code boots in Chrome and Firefox.
- A user can log in, select a character, enter a map, move, and chat.
- Native and web builds use the same packet handlers and UI classes.

### Phase 3: Core Gameplay Parity

Estimate: additional 12-18 engineer-weeks

Deliverables:

- Combat, skills, buffs, mobs, drops, reactors, and map transitions.
- Inventory, equipment, items, NPC dialogue, shops, and storage.
- Quests, quest helper, and NPC indicators.
- Party, buddy, and guild basics.
- Pets, mounts, summons, doors, mists, and common field effects.
- Character creation and PIC flow verification.
- Automated browser smoke tests for representative progression.

Exit criteria:

- The normal early-game progression loop is usable against the pinned server.
- No known packet desynchronization exists in accepted core flows.
- Chrome and Firefox pass a sustained-session test.

Cumulative estimate through core parity: 29-45 engineer-weeks.

### Phase 4: Broad OpenStory Parity

Estimate: additional 20-30 engineer-weeks

Deliverables:

- Full party, buddy, guild, alliance, family, messenger, and social UI.
- Trade, personal shops, hired merchants, parcels, and Owl of Minerva.
- Cash shop and MTS where supported by the pinned server.
- Monster Book, MapleTV, reports, megaphones, and secondary windows.
- Supported v83 events and minigames.
- Gamepad support through browser APIs.
- Spatial audio through Web Audio.
- Unicode, RTL, fallback fonts, emoji, inline icons, and text editing parity.
- UI scaling, high-DPI handling, screenshots, and performance work.

Exit criteria:

- Every accepted stock OpenStory feature is functional or explicitly disabled by server capability.
- Asset contracts and missing-node behavior exist for every migrated UI.
- No high-severity browser-specific regression remains open.

Cumulative estimate through broad parity: 49-75 engineer-weeks, approximately 12-19 person-months.

### Phase 5: Production Hardening

Estimate: additional 24-40 engineer-weeks

Deliverables:

- Hardened WSS gateway with destination allowlists and abuse controls.
- Immutable CDN asset delivery and reliable cache invalidation.
- Metrics, structured logs, browser crash reporting, and packet diagnostics.
- Cross-browser qualification and lower-end-device performance budgets.
- Long-session, reconnect, memory, and storage tests.
- Visual regression coverage for major interfaces.
- Versioned releases, rollback, and deployment documentation.
- Security review and operational runbooks.
- AGPL distribution/source-offer process and explicit asset-provenance policy.

Exit criteria:

- The client can be exposed publicly without acting as an open TCP relay.
- Releases are reproducible and rollbackable.
- Supported browsers pass the acceptance suite.
- Operators can identify protocol, asset, gateway, and client failures separately.

Cumulative estimate through production readiness: 73-115 engineer-weeks, approximately 18-29 person-months.

### Phase 6: Optional Custom Systems

Estimate: additional 12-24 engineer-weeks

Candidate scope:

- Custom Event System.
- Monster Life and Monster Battle phases.
- Bot inventory and bot movement.
- Procedural equipment and custom jobs.

Each system requires a separate protocol contract, server capability, asset contract, tests, and security review. These systems must not block stock-v83 web readiness.

## First Vertical Slice

Before committing to the full schedule, run a two-engineer, two-week architecture spike.

The spike should include:

- One Emscripten build of shared OpenStory code.
- One binary WebSocket connection to a locally pinned Cosmic server.
- One asynchronously loaded NX asset subset.
- One packet-heavy UI, preferably Quest Log/Quest Helper or Buddy/Guild.
- One WebGL2-rendered map and UI overlay.
- One packet fixture and one Playwright smoke test.

The spike should answer:

- How much of `GraphicsGL` compiles unchanged under WebGL2?
- Can NoLifeNx operate efficiently over asynchronously populated memory?
- Which shared singletons require lifecycle changes first?
- Which OpenStory UI nodes are absent from the chosen asset set?
- What WASM memory budget is needed after entering a representative map?
- How much packet behavior differs from the pinned Cosmic revision?

Use the results to revise estimates before starting Phase 2.

## Migration Rules

- Preserve OpenStory behavior before improving its internal design.
- Keep native and web builds green in every migration PR.
- Do not create parallel native and web packet handlers.
- Do not put game rules in TypeScript or the gateway.
- Do not copy native socket, BASS, filesystem, or sleep-loop code into web conditionals.
- Do not add Asyncify to conceal blocking APIs.
- Do not port plaintext password persistence.
- Do not expose custom protocols without negotiated server capability.
- Add a packet fixture whenever a packet layout is changed.
- Add an asset contract or fallback whenever a new UI asset path is introduced.
- Record architecture decisions that change these boundaries.

## Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| OpenStory and the existing WASM prototype have no usable merge base | Feature copying becomes conflict-heavy | Port browser adapters into OpenStory rather than merging gameplay trees |
| UI assets mix v83 and newer/custom nodes | Runtime failures despite successful compilation | Pin a manifest, validate nodes, and test missing-node behavior |
| Cosmic forks differ in packet layouts | Silent state corruption or disconnects | Pin one server revision and use packet contract tests |
| Legacy OpenGL behavior differs under WebGL2 | Rendering defects or build failures | Introduce an explicit WebGL2 path and visual baselines early |
| Browser tabs are throttled or suspended | Physics catch-up and network desynchronization | Clamp elapsed time, pause rendering, and implement reconnect/resync states |
| WASM memory growth causes stalls | Poor map transitions or crashes | Establish budgets, instrument allocations, and use deterministic eviction |
| Asset delivery is legally or operationally unavailable | Users cannot start the client | Separate code from assets and support authorized hosting or local import |
| A public gateway becomes an open relay | Infrastructure abuse and security exposure | Fixed upstream allowlists, authentication, origin checks, and rate limits |
| Shared singleton coupling slows tests | High regression cost | Add seams around packet effects incrementally instead of a full rewrite |
| Custom features inflate the critical path | Delayed stock-v83 release | Capability-gate and defer them to Phase 6 |

## Success Metrics

Correctness:

- Zero known packet desynchronization in the acceptance suite.
- Native and web packet fixtures produce equivalent state.
- Major UI screenshot baselines pass at supported resolutions.

Performance:

- Startup and first-map budgets are defined after the vertical slice.
- Map transitions do not trigger unbounded WASM or GPU memory growth.
- Sustained gameplay maintains the selected frame-time budget on reference hardware.

Reliability:

- Reconnect and channel transfer pass automated scenarios.
- Background-tab recovery does not fast-forward physics.
- Asset cache upgrades are deterministic and recoverable.

Operations:

- Gateway, asset, protocol, and browser failures are distinguishable in telemetry.
- Every production deployment references immutable client and asset versions.
- Rollback does not require rebuilding artifacts.

## Initial Architecture Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Authoritative implementation | OpenStory | It has the broadest current gameplay and Cosmic coverage |
| Shared implementation language | C++17 | Preserves working gameplay and UI while supporting Emscripten |
| Browser renderer | WebGL2 first | Lowest-risk path from current OpenGL; WebGPU can follow |
| In-game UI | Shared canvas UI | Avoids maintaining a second DOM implementation |
| Browser transport | Binary WebSocket | Matches the ordered TCP protocol through a gateway |
| Asset transport | HTTPS with immutable hashes | Uses browser/CDN caching and avoids a custom asset socket |
| Browser execution | Worker plus `OffscreenCanvas` where supported | Reduces main-thread contention and isolates the game loop |
| Async model | Explicit lifecycle and requests | Avoids Asyncify overhead and hidden blocking behavior |
| First server target | One pinned Cosmic revision | Makes packet contracts testable and reproducible |
| Custom features | Capability-gated and deferred | Prevents server-specific experiments from blocking parity |

## Open Questions

- Which Cosmic commit and configuration will define the first compatibility profile?
- Which asset versions and UI nodes are authorized and known to work with all accepted features?
- Will the first release allow local asset import, operator-hosted assets, or both?
- Is Safari required for the first public release or a later qualification milestone?
- Is mobile browser support a product requirement?
- Which OpenStory custom systems are actual product requirements?
- What account/session mechanism should authorize gateway connections?
- What startup, memory, and frame-time budgets define acceptance?
- Should native builds continue using raw NX files after the web content pipeline exists?
- Where will sanitized protocol replays and non-proprietary fixture assets be stored?

Resolve these questions during Phase 0 and record material choices as architecture decision records under `docs/adr/`.
