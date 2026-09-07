# GD Helper

A Geometry Dash [Geode](https://geode-sdk.org/) mod that records your play sessions to JSON so you can see **where** you die, **what** killed you, and later compare attempts against a tagged **perfect run**.

This is an analysis tool, not a clickbot. It does not play the game for you.

Mod ID: `gdhelper.analyzer`  
Target: Geometry Dash **2.2081** on **Windows**, Geode **5.10.1**

## How this works

```
your attempt ──► Geode hooks ──► session JSON ──► where / what / (later) why
perfect run  ──►     same     ──► reference JSON ─┘
```

| Question | What v0.2 records | Later |
| --- | --- | --- |
| Where do I fail most? | Death `x`, `y`, `percent` per attempt | Heatmaps / clustering |
| What object / mode? | Killer `objectId` + `GameObjectType`, gamemode | Readable names for spikes vs solids |
| Am I above/below the good line? | Sampled path (`x`, `y` every 8 frames) plus a tagged `reference` completion | Diff `y` vs the reference at the same `x` |
| Where did I click? | `handleButton` press/release with `x`, `y`, `percent`, `frame` | Compare click timing to the reference |
| Generate a perfect run for me | Out of scope | Use your first completion, or import a GDR later |

A “perfect run” here is **your first normal-mode completion** of that level, copied to `references/`. It is a reference path, not the only valid route. Physics also has to match (vanilla 240 TPS, same practice / start-pos / CBF settings).

Classic (non-platformer) levels only for now. Platformer plays are skipped and logged.

## Prerequisites (Windows)

Follow the official [Getting Started](https://docs.geode-sdk.org/getting-started/) guide. In short:

1. Install [Geometry Dash](https://store.steampowered.com/app/322170/Geometry_Dash/) and the [Geode loader](https://geode-sdk.org/install).
2. Install **Git**, **CMake 3.29+** (add it to PATH), and **Visual Studio 2022 Build Tools** with the **Desktop development with C++** workload (MSVC + Windows SDK).
3. Recommended: LLVM and Ninja via [Scoop](https://scoop.sh/):

   ```powershell
   scoop install llvm ninja
   ```

4. Install the [Geode CLI](https://docs.geode-sdk.org/getting-started/geode-cli/). With Scoop:

   ```powershell
   scoop bucket add extras
   scoop install geode-sdk/geode
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

Use the same popup to stitch a **golden run** from StartPos sections: play until a good pause point, **Commit Segment**, then play from a StartPos **before** that seam and wait until status is `Recording Segment` before continuing. Death before commit discards only the in-progress segment.

### Mapping a level that already has StartPos objects

Work **forward** through the StartPos list. The stitcher only extends the golden run; it cannot fill a gap behind the last commit.

1. Play from the first StartPos (or 0%). Get a clean run to a stable point **past** that section — typically just before or at StartPos 2. Pause → Golden Run → **Commit Segment**. Mapped % should jump.
2. Restart from a StartPos **behind** that commit (StartPos 1, or 2 if it is still before the seam). Status should be `Waiting for Seam Alignment`.
3. Play the pre-roll until status flips to `Recording Segment` (physics matched the last commit). Keep going into the next new section.
4. Pause and commit again. Repeat: always start from StartPos *N*, commit somewhere toward StartPos *N+1*.
5. If you die before committing, only the current attempt is thrown away. Previous commits stay.
6. Turn **Record Session** off if you only want the golden click sequence and not the large per-attempt JSON files.

If you start from a StartPos that is already **ahead** of the mapped seam, alignment will never fire. Go back one StartPos.

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

- `PlayLayer::init` — start a session from `GJGameLevel`, load any golden run for the level
- `PlayLayer::resetLevel` — new attempt (manual restarts are stored as `outcome: "reset"`); stitcher starts recording or waits for a seam
- `PlayLayer::postUpdate` — sample player `x`/`y`/`percent` every 8 frames; match seam state while waiting
- `PlayLayer::destroyPlayer` — first frame of a real player death (dummy objects ignored); invalidates an uncommitted golden segment
- `PlayLayer::levelComplete` — `outcome: "complete"`; first non-practice, non-start-pos win becomes the reference; commits a recording golden segment
- `PlayLayer::onQuit` — flush JSON and persist the golden run
- `GJBaseGameLayer::handleButton` — log jump/left/right press and release; golden clicks are recorded only after seam alignment
- `PauseLayer::customSetup` — **Golden Run** button opens the popup (coverage, Record Session toggle, stitcher status, commit/clear)

Each attempt JSON object includes `path` (sampled trajectory) and `clicks` (inputs for that attempt). Schema version is `1`. Session recording can be disabled from the Golden Run popup or Geode settings (`record-sessions`).

## Roadmap

After golden-run stitching:

1. In-game death markers on the progress bar.
2. A small offline comparer (Python is fine) that diffs a session against the tagged reference: death clusters, Y error vs the good line, then input timing.

Related mods if you want to explore the space: [DeathMarkers](https://github.com/MaSp005/deathmarkers), [BetterStats](https://geode-sdk.org/mods/logon.betterstats), [ToastyReplay](https://github.com/ToastexGD/ToastyReplay) / [GDR](https://github.com/maxnut/GDReplayFormat).

## Resources

- [Geode SDK docs](https://docs.geode-sdk.org/)
- [Creating a mod](https://docs.geode-sdk.org/getting-started/create-mod/)
- [Geode GitHub](https://github.com/geode-sdk/geode)
- [Bindings (`GeometryDash.bro`)](https://github.com/geode-sdk/bindings/)
- [DevTools](https://github.com/geode-sdk/DevTools) — useful for inspecting `PlayLayer` in-game
