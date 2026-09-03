# GD Helper

Records your Geometry Dash attempts to JSON so you can study **where** you die and **what** killed you.

This is not a clickbot. It watches `PlayLayer` (deaths, resets, completions) and writes session files under this mod's save directory.

## What you get in v0.1

- One JSON file per classic-level session
- Each attempt stores percent, position, gamemode, and the collider that killed you
- Your first normal-mode completion of a level is tagged as the **reference** (perfect run) for later comparison
- Platformer levels are ignored for now

## Where the files go

`geode/mods/gdhelper.analyzer/sessions/`  
`geode/mods/gdhelper.analyzer/references/` (first completion per level)

See the GitHub README for setup, the JSON contract, and the analysis roadmap.
