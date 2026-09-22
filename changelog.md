# v0.4.0

- Golden Run auto-saves when you pass the next StartPos after the one you started from, so you can continue from that StartPos instead of pausing to commit and replaying the previous section
- Golden Run popup shows the next StartPos auto-save target

# v0.3.1

- Fix Golden Run coverage bar overflowing the popup
- Fix Record Session checkbox being inverted vs the on/off toast

# v0.3.0

- Segment stitcher builds a golden click sequence from StartPos sections, waiting for seam alignment (x/y/y-accel/upside-down) before recording
- Pause menu **Golden Run** popup: coverage bar, session-record toggle, stitcher status, commit/clear

# v0.2.1

- Fix death percent reading ~0 on the `destroyPlayer` frame because of Geometry Dash's anticheat dummy object

# v0.2.0

- Record deaths, resets, and completions to per-session JSON
- Sample player path (`x`, `y`, percent, gamemode) every 8 frames
- Log press/release clicks with position and percent
- Tag the first normal-mode completion of a level as a reference run
