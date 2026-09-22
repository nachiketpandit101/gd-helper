# GD Helper

A Geometry Dash [Geode](https://geode-sdk.org/) mod that records play sessions to JSON so you can see **where** you die, **what** killed you, and **where you clicked**, then stitch a **golden run** from StartPos sections.

This is an analysis tool, not a clickbot. It does not inject inputs or play the game for you.

| Field | Value |
| --- | --- |
| Language | C++23 |
| SDK | [Geode](https://geode-sdk.org/) 5.10.1 |
| Target | Geometry Dash **2.2081** (Windows) |
| Mod ID | `gdhelper.analyzer` |
| Version | 0.4.0 |

## How this works

```
your attempt ──► Geode hooks ──► session JSON ──► where / what / (later) why
perfect run  ──►     same     ──► reference JSON ─┘
```

| Question                        | What it records                                                             | Later                                            |
| ------------------------------- | --------------------------------------------------------------------------- | ------------------------------------------------ |
| Where do I fail most?           | Death `x`, `y`, `percent` per attempt                                       | Heatmaps / clustering                            |
| What object / mode?             | Killer `objectId` + `GameObjectType`, gamemode                              | Readable names for spikes vs solids              |
| Am I above/below the good line? | Sampled path (`x`, `y` every 8 frames) plus a tagged `reference` completion | Diff `y` vs the reference at the same `x`        |
| Where did I click?              | `handleButton` press/release with `x`, `y`, `percent`, `frame`              | Compare click timing to the reference            |
| Generate a perfect run for me   | Out of scope                                                                | Use your first completion, or import a GDR later |

A “perfect run” here is **your first normal-mode completion** of that level, copied to `references/`. It is a reference path, not the only valid route. Physics also has to match (vanilla 240 TPS, same practice / start-pos / CBF settings).

Classic (non-platformer) levels only for now. Platformer plays are skipped and logged.

## Prerequisites (Windows)

Follow the official [Getting Started](https://docs.geode-sdk.org/getting-started/) guide. In short:

1. Install [Geometry Dash](https://store.steampowered.com/app/322170/Geometry_Dash/) and the [Geode loader](https://geode-sdk.org/install).
2. Install **Git**, **CMake 3.21+** (add it to PATH), and **Visual Studio 2022 Build Tools** with the **Desktop development with C++** workload (MSVC + Windows SDK).
3. Recommended: LLVM and Ninja via [Scoop](https://scoop.sh/):

   ```powershell
   scoop install llvm ninja
   ```

4. Install the [Geode CLI](https://docs.geode-sdk.org/getting-started/geode-cli/). With Scoop:

   ```powershell
   scoop bucket add extras
   scoop install geode-sdk-cli
   geode --version
   ```

5. Install the SDK and prebuilt binaries:

   ```powershell
   geode sdk install
   geode sdk install-binaries
   ```

   Confirm `$env:GEODE_SDK` is set (restart the terminal after install).

6. Point the CLI at your GD install so builds drop into the game:

   ```powershell
   geode config setup
   ```

The `geode` field in [`mod.json`](mod.json) must match your installed SDK **major.minor** (currently `5.10.1`). If `geode build` errors about a version mismatch, run `geode sdk --help` / check your SDK version and update that field.

## Build

```powershell
cd gd-helper
geode build
```

If you installed Clang + Ninja:

```powershell
geode build --ninja
```

Or raw CMake:

```powershell
cmake -B build
cmake --build build --config RelWithDebInfo
```

A profile from `geode config setup` installs `gdhelper.analyzer.geode` into GD automatically. Otherwise copy the `.geode` package from `build/` into your Geode mods folder.

## Verify a session

1. Launch Geometry Dash with Geode.
2. Play any classic level, die a few times, then beat it (or quit).
3. Open:

   `%LOCALAPPDATA%\GeometryDash\geode\mods\gdhelper.analyzer\sessions\`

   Each visit to a level writes `session-{id}-{name}-{timestamp}.json`.

4. After your **first normal-mode completion** of a level, the same JSON is also copied to:

   `%LOCALAPPDATA%\GeometryDash\geode\mods\gdhelper.analyzer\references\`

   with `"reference": true`. That file is the perfect-run baseline for later comparison.

An annotated example lives in [`docs/session.example.json`](docs/session.example.json).

Path and click logs make session files much larger. Pause the level and open **Golden Run** (or Geode → GD Helper settings) to turn **Record Session** off when you are not analyzing a run. Turning it off stops sampling immediately and flushes the current session; turning it back on starts a new file.

## Golden Run

Use the pause-menu **Golden Run** popup to stitch a click sequence from StartPos sections. The stitcher scans StartPos objects when the level loads. While you are recording, passing the **next** StartPos after the one you started from auto-saves that segment and keeps recording, so a clean run can map several sections in one attempt.

Death before the next StartPos discards only the in-progress segment. Previous auto-saves stay. Restart from the StartPos that was just saved — recording starts immediately from there (no need to replay the previous section to match physics).

**Commit Segment** is still there for the last stretch after the final StartPos, or for levels with no StartPos objects. Finishing the level also commits.

### Mapping a level that already has StartPos objects

Work **forward** through the StartPos list. The stitcher only extends the golden run; it cannot fill a gap behind the last save.

1. Play from the first StartPos (or 0%). Status should be `Recording Segment`. The popup shows the next auto-save target (`Auto-save: SP 2/N`).
2. Get a clean run until you pass that next StartPos. A toast confirms the save; mapped % jumps. Keep going — the next StartPos is now the target.
3. If you die, restart from the last saved StartPos. Status should be `Recording Segment` right away.
4. Repeat until the popup says `Last section - commit or finish`. Beat the level, or pause and **Commit Segment**.
5. Turn **Record Session** off if you only want the golden click sequence and not the large per-attempt JSON files.

If you start from a StartPos that is already **ahead** of the mapped seam, recording will not start. Go back to the last saved StartPos.

Starting from an earlier StartPos still waits for the old physics seam (`Waiting for Seam Alignment`) if you want that path. The usual flow is to start from the StartPos you just saved to.

Golden click sequences are stored under:

`%LOCALAPPDATA%\GeometryDash\geode\mods\gdhelper.analyzer\golden\`

## Project layout

```
CMakeLists.txt          Geode / CMake project
mod.json                Mod metadata (id gdhelper.analyzer)
src/main.cpp            PlayLayer + GJBaseGameLayer + PauseLayer hooks
src/SessionRecorder.*   Session state + JSON writer
src/GoldenStitcher.*    StartPos seam matching + golden input sequence
src/GoldenRunPopup.*    Pause-menu Golden Run popup
docs/session.example.json
```

Hooks used:

- `PlayLayer::init` — start a session from `GJGameLevel`, load any golden run for the level, scan StartPos objects
- `PlayLayer::resetLevel` — new attempt (manual restarts are stored as `outcome: "reset"`); stitcher starts recording or waits for a seam
- `PlayLayer::postUpdate` — sample player `x`/`y`/`percent` every 8 frames; match seam state while waiting; auto-save a golden segment when the next StartPos is passed
- `PlayLayer::destroyPlayer` — first frame of a real player death (dummy objects ignored); invalidates an uncommitted golden segment
- `PlayLayer::levelComplete` — `outcome: "complete"`; first non-practice, non-start-pos win becomes the reference; commits a recording golden segment
- `PlayLayer::onQuit` — flush JSON and persist the golden run
- `GJBaseGameLayer::handleButton` — log jump/left/right press and release; golden clicks are recorded only after seam alignment
- `PauseLayer::customSetup` — **Golden Run** button opens the popup (coverage, next StartPos auto-save, Record Session toggle, stitcher status, commit/clear)

Each attempt JSON object includes `path` (sampled trajectory) and `clicks` (inputs for that attempt). Schema version is `1`. Session recording can be disabled from the Golden Run popup or Geode settings (`record-sessions`).

## Resources

- [Geode SDK docs](https://docs.geode-sdk.org/)
- [Creating a mod](https://docs.geode-sdk.org/getting-started/create-mod/)
- [Geode GitHub](https://github.com/geode-sdk/geode)
- [Bindings (`GeometryDash.bro`)](https://github.com/geode-sdk/bindings/)
- [DevTools](https://github.com/geode-sdk/DevTools) — useful for inspecting `PlayLayer` in-game
