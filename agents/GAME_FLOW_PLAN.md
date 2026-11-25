# Game Flow Plan

## Goal

Establish a maintainable game-flow foundation for a Sega Saturn game built with
Jo Engine. The intended game combines a 2D platformer authored with
JoMapEditor, 3D backgrounds and possible 3D foreground effects, and CD music
that can change during a level. It has two playable characters with mostly
shared gameplay and small character-specific differences, approximately six
zones with two levels each, a boss at the end of each zone's second level, and
a final boss. Scripted cutscenes with dialogue and character portraits appear
at selected points, especially zone openings and before boss battles. An
after-story mode will become selectable from the main menu when TBD completion
milestones have been met. Each character also has a gameplay meter that can
trigger a powered-up state; in regular levels, that state switches the CD
music to that character's alternate version of the zone music.

This is an architecture and implementation sequence, not a claim that the
features are already implemented.

## Game loop and states

Initialize Jo Engine and shared resources in `jo_main()`, register one regular
game-loop callback, then call `jo_core_run()`. Keep the callback registered for
the lifetime of the program and have it dispatch to the active game state.
This keeps state changes explicit and avoids registering/removing ordinary
callbacks every time a screen changes.

Initial states:

```text
BOOT -> MAIN_MENU -> CHARACTER_SELECT -> LEVEL_TRANSITION -> PLAYING
          +-> AFTER_STORY (only when unlocked)

PLAYING <-> PAUSED
PLAYING --meter full--> POWERED_UP --power-up ends--> PLAYING
   |
   +-> CUTSCENE -> resume / next scene / boss encounter
   |
   +-> LEVEL_TRANSITION -> next level or next zone
                               |
                               +-> optional CUTSCENE -> PLAYING
                               |
                               +-> zone 6 boss defeated -> FINAL BOSS
                                                             |
                                                       ENDING / CREDITS
                                                             |
                                              update completion milestones

PLAYING -> GAME_OVER -> RETRY / CHARACTER_SELECT / MAIN_MENU
```

`POWERED_UP` is a gameplay substate of `PLAYING`, not a separate campaign
destination. It can coexist with pause handling; while a cutscene or boss
encounter is active, its own state/music rules take precedence as described
below.

The first playable build can start directly in `PLAYING`; the other states
define the flow to grow into. Store the selected character and current
zone/level and completion milestones as game progression state, separate from
the temporary active scene. Represent a requested transition separately from
the active state and apply it at a consistent point in the loop, so an input,
update, or draw operation cannot unexpectedly replace the state in the middle
of its work.

A regular frame should have an intentional order:

1. Read input.
2. Update the active state's game logic.
3. Apply any requested state or level transition.
4. Draw the active scene.

The exact Jo Engine callback timing should be confirmed with a small prototype
before relying on frame-timing assumptions.

## Frame-rate-independent gameplay

PAL and NTSC builds refresh at 50 Hz and 60 Hz respectively. With the current
`JO_FRAMERATE = 1` setting, the game callback runs once per VBlank, so code
that advances physics or timers by a fixed amount per callback runs about 20%
faster in NTSC than PAL. The current prototype's acceleration, gravity,
horizontal scrolling, and animation cadence are frame-based and therefore do
not yet have equivalent real-time behavior in both versions.

Before expanding gameplay, make simulation and timed behavior independent of
the video standard. Choose and document one timing approach—elapsed-time
scaled updates or a fixed simulation step with an accumulator—and apply it
consistently to movement, acceleration, gravity, jump behavior, animations,
power-up duration/meter gain, invulnerability/cooldowns, cutscene timing, and
other gameplay timers. Input should be sampled in a way that remains
responsive if a video frame runs zero or multiple simulation steps. Avoid
fixing this with unrelated PAL-specific constants scattered through gameplay.

Validate both builds using real elapsed time, not just frame counts. Compare
time to reach top speed, jump airtime and height, distance traveled, animation
duration, and timed gameplay events between PAL and NTSC. Differences should
stay within an explicitly chosen tolerance while each version renders at its
own regional refresh rate.

## Responsibilities and likely source layout

Start with a small number of modules and split only as the code needs it:

- `main.c` — Jo Engine initialization and callback registration.
- `src/game.c/.h` — application game setup, game-loop orchestration, and the
  current game reset path.
- `src/game_state.h` — runtime state shared by game, input, player, and level code.
- `src/input.c/.h` — controller polling and movement input.
- `src/level.c/.h` — map and texture loading, reset, and drawing.
- `src/player.c/.h` — shared movement, collision, animation, and player drawing.
- `game_flow.c/.h` — active state, transition requests, and state lifecycle.
- `level.c/.h` — zone/level metadata, map loading, and spawn/camera setup.
- `player.c/.h` — shared movement and collision logic, parameterized by
  selected character, including that character's power meter and powered-up
  state.
- `character.c/.h` — selected character definition and the small set of
  character-specific assets or gameplay parameters, if these differences
  warrant a separate module.
- `boss.c/.h` — boss encounter state and behavior, once the first boss
  prototype establishes what can be shared.
- `cutscene.c/.h` — scripted scene progression, dialogue text, portrait
  selection, and cutscene completion/skip handling.
- `progression.c/.h` — campaign completion flags and after-story unlock
  evaluation, if not kept within `game_flow.c`.
- `background_3d.c/.h` — 3D camera, background scene, and optional effects.
- `game_audio.c/.h` — mapping game music identifiers to CD tracks and
  controlling playback in response to level, power-up, and boss state.

These are responsibility boundaries, not a requirement to create every file
immediately. Keep the two characters on one movement/collision path and
represent their differences as explicit data where possible; only split out
character-specific logic if the differences become behaviorally distinct.
Likewise, avoid duplicating whole levels for bosses if the boss can be modeled
as an encounter at the end of a level. Each added C source file must also be
listed in `SRCS` in the project `makefile`.

The power meter and active powered-up state belong to the selected player
state. Keep meter accumulation and activation/deactivation rules explicit and
independent from CD track numbering; notify the audio controller when the
power state changes rather than making player code directly issue arbitrary
track commands.

Cutscenes should be separate from ordinary gameplay updates: while one is
active, advance its script and dialogue rather than continuing player physics
or boss AI unless a particular scripted event deliberately needs them. Keep
the cutscene system data-driven enough to reuse a common dialogue box and
portrait presentation for zone introductions and boss lead-ins.

## Character and progression model

Keep character selection separate from zone/level progression. The selected
character should persist as the player advances through levels and zones,
unless a deliberate story or game-over flow changes it. Both characters share
the same core movement, collision, camera, and level-transition systems, but
their combat actions are behaviorally distinct. Keep the shared controller
and dispatch character-specific actions through explicit character data or
small action handlers rather than duplicating the whole player controller.

The current character design is:

- **REI** is ranged and shoots arrows in the direction pressed. Moving while
  pressing down performs a roll with invulnerability; REI can shoot during
  the roll. Shooting down while airborne performs a pseudo-double-jump.
- **SAM** is melee. Moving while pressing down performs a damaging slide.
  SAM's air spin attack damages enemies and bounces off them.
- Both characters share the `player new jump` jump animation and the
  `player wall land` wall-cling animation.
- Current prototype controls are D-pad movement, C to jump, B to trigger an
  attack animation, X to switch characters, Y to toggle the profiler, and
  START to reset the level. Attack button behavior is animation-only for now;
  projectile, damage, invulnerability, and enemy-bounce mechanics are not yet
  implemented.

Planned animation sheets, sourced from `projFiles/Nov 16th (woodcutting
animation added)/`, belong to these character sets:

| Character | Action | Animation sheet |
|-----------|--------|-----------------|
| REI | Roll | `Roll Quick/Player Roll 48x48.png` |
| REI | Stationary shooting | `Shooting (two-handed)/player shoot 2H 48x48.png` |
| REI | Running/jumping shooting | `Shooting (running and aiming)/Player Running Shooting 48x48.png` |
| SAM | Slide | `Slide/Player Slide 48x48.png` |
| SAM | Ground attack | `Sword Stab/Player Sword Stab 96x48.png` |
| SAM | Air attack | `Air Spin/player air spin 48x48.png` |
| Both | Jump | `Jump/player new jump 48x48.png` |
| Both | Wall cling | `Wall Land/player wall land 48x48.png` |

Jo Engine sprite memory is limited VDP1 VRAM. Keep animation frame pixels for
the selected character in main RAM and use a single replaceable 48 x 48 VDP1
sprite for drawing, uploading a frame only when the animation changes frame.
Load the next character's sheets into the same RAM cache when switching; pause
CD audio during file access. Crop the 96 x 48 sword-stab source frames to
centered 48 x 48 frames for the shared sprite slot. The player animation cache
is currently a fixed 270 KiB allocation; keep its capacity and the Jo Engine
dynamic heap budget in mind as animations are added.

Keep the shared action definitions and behavior consistent between characters.
Exact directional-input rules and action timing,
cooldowns, hitboxes, invulnerability duration, and enemy-bounce response still
need to be specified and tested when gameplay implementation begins.

Each character has its own meter that fills during gameplay. When full, it
triggers that character's powered-up state, which may change the character's
abilities/appearance and changes the regular level music to that character's
alternate zone track. The meter's fill sources/rate, powered-up duration, and
how/when the meter resets are still design decisions; keep these tunable
without building them into the audio mapping. Reaching full meter
automatically enters the powered-up state.

Represent the campaign with explicit zone and level identifiers rather than
inferring the next stage from file names or scattered conditionals. The
planned content is roughly six zones with two levels apiece. The second level
of each zone ends in that zone's boss encounter; defeating it advances to the
next zone. After the sixth zone boss, transition to the separate final boss,
then the ending. Keep the approximate zone count easy to adjust while the
content plan is still evolving.

Level metadata should identify at least the map/assets, spawn point, music,
and whether/how the level ends in a boss encounter. The flow layer should
resolve completion to the next level, next zone, final boss, or ending, while
the level and boss systems own their local gameplay. Prototype the first
zone's two-level and boss progression before implementing all campaign
content.

## Cutscenes and dialogue

Treat a cutscene as a flow mode that can be entered from a level transition or
gameplay trigger and returns to a defined destination when complete. The
initial use cases are selected zone openings and pre-boss scenes; not every
level needs a cutscene. A scene should be able to sequence simple scripted
actions and speaker dialogue, show the speaker's portrait beside a text box,
advance lines with player input, and then hand control back to gameplay or
start the boss encounter.

Keep dialogue presentation (text box, portrait, speaker/name, line advance)
separate from scene-specific sequencing. Define each scene's completion
destination explicitly so a pre-boss scene starts the encounter and a zone
opening scene enters the appropriate level. Whether scenes can be skipped,
which inputs advance them, and whether any scenes branch based on selected
character are design decisions to settle before implementing the full system.
Prototype a short, linear scene first.

## After-story and unlock progression

Add an after-story option to the main menu, but show it as selectable only
after the required completion milestone or milestones have been satisfied.
The exact criteria are TBD; keep the criteria in one progression/unlock rule
instead of scattering checks through the menu and game flow. Update campaign
completion flags at defined events such as boss defeat or ending completion,
then evaluate the unlock from those flags.

The after-story should be a distinct game mode with its own entry/exit flow,
while reusing shared systems such as character selection, level loading,
cutscenes, and music where appropriate. Decide separately whether
after-story unlocks must persist across power-off; if so, connect the
milestone data to an appropriate save mechanism. Do not assume a persistence
format or unlock threshold until those requirements are defined.

## Level and platformer flow

Treat each JoMapEditor `.MAP` as a sprite map. A level setup should load the
required sprite/image resources, load its map into a known map layer, initialize
the player at a level-defined spawn point, set the camera/map offset, and
select the initial music.

Keep player world position distinct from the map's screen offset. Use the map
draw offset for camera scrolling, and consistently translate player/world
coordinates when calling Jo Engine's map collision helpers. Jo Engine's map
collision functions accept screen-relative coordinates, so this coordinate
convention needs to be documented in the platformer code.

Level/region triggers and boss defeat should request a transition. The
transition handler is responsible for advancing zone/level identifiers,
setting up the destination, and updating music once, not every frame. When
changing levels, handle any required unloading/reloading deliberately and
preserve the selected character and intended campaign state; reset
level-specific player/camera state as appropriate.

## Rendering composition

Keep 3D background rendering and 2D platform gameplay as separate
responsibilities. Prototype the intended ordering on hardware/emulators:

1. Draw/update the 3D background scene.
2. Draw the scrolling 2D map.
3. Draw the player and other gameplay sprites.
4. Draw foreground effects and HUD elements in their intended depth/order.

This is a proposal, not a guarantee of render-layer behavior: validate that
the selected Jo Engine APIs and Saturn VDP1/VDP2 composition produce the desired
result. Treat 3D foreground effects as a later prototype until the background,
map, and player composition is confirmed.

## CD music flow

Give each zone, act, character power-up track, and boss encounter a stable
game-side music identifier and keep the mapping from identifier to CD track in
`game_audio.c`. The planned track set is:

- One regular level-music track per zone, with the option to provide a
  different regular track for each act if disc capacity permits.
- Two powered-up alternate tracks per zone, one for each playable character.
  These replace the regular zone/act track while that character is powered up
  during regular level gameplay.
- A boss theme for each level that ends in a boss encounter, plus the final
  boss. Based on the current campaign structure, that is six zone-boss themes
  and one final-boss theme.

Use the following priority so the power-up music does not replace a boss
encounter's required theme:

1. Boss encounter theme.
2. Powered-up alternate for the selected character in regular level gameplay.
3. Regular music for the current zone/act.

When a power-up starts or ends, the audio controller selects the corresponding
track once. During a boss encounter, the boss theme remains active even if the
character powers up; after the boss encounter, the next state selects the
appropriate zone/act music. Decide and test whether returning from an
alternate track restarts the regular track or follows another supported
continuity strategy.

This implies a preliminary music-track budget of about 25 tracks with one
regular track per zone (6 regular + 12 character alternates + 7 boss
encounters), or about 31 if regular music differs by act (12 + 12 + 7).
This excludes menu/after-story/cutscene music, sound effects, and any extra
variations. Treat it as a planning estimate: confirm actual CD image capacity
using final track durations and the build output before producing the whole
score.

Start or change a track on a state, region, or power-state transition, with a
check to avoid restarting the same track every frame.

Jo Engine numbers the game data as CD track 1 and audio tracks from 2. Track
ordering is derived from audio filenames in case-insensitive alphabetical
order, so choose and maintain a naming convention and inspect the generated
`game.cue` whenever the track list changes.

CD playback and file loading share the CD drive. If a level transition loads
files from the disc while music is playing, explicitly handle the playback
state around loading (for example pause, load, then resume or start the new
level's track). Test this behavior in the target emulators and on hardware if
available.

## First vertical-slice milestone

Build the smallest scene that exercises all three technical goals:

1. Start directly in a test level with either selected character.
2. Verify both characters use the same controller and that their intended
   differences are represented without duplicating core movement/collision.
3. Load and scroll a JoMapEditor map; verify ground and wall collision.
4. Draw a simple 3D background behind the 2D playfield.
5. Fill the meter, enter powered-up state, and verify the selected character's
   alternate zone track plays; leave powered-up state and verify regular zone
   music is restored.
6. Play a short scripted pre-boss cutscene with dialogue and a portrait, then
   enter a simple boss encounter and verify the boss theme takes priority even
   if the character powers up.
7. Verify music changes happen at intended boundaries and gameplay pauses
   correctly during the cutscene.
8. Confirm transitions and rendering in an emulator.

### Current prototype status

The current prototype is split across `src/game.c`, `src/input.c`,
`src/player.c`, and `src/level.c`; `main.c` is limited to Jo Engine initialization and starting the
game. It starts directly in `DEMO2.MAP`, uses one shared movement/collision
path, and lets X switch between two temporary character profiles. Both
profiles currently reuse `SNC.BIN`; their only test difference is max speed
(6 versus 5), so replace these placeholders with the actual character assets
and design parameters when available. The map scrolls with the player and
uses Jo Engine's vertical and hitbox collision helpers. CD audio is also
enabled and the test `audio/track1.wav` is started as CD track 2.

The vertical slice should prove the shared character path and a representative
level-to-boss progression and one cutscene handoff, not implement all six
zones, every scene, the final boss, or after-story content. Add the
character-select screen, full campaign, distinct boss designs, final boss,
after-story mode, and detailed effects after the core flow and main
rendering/audio constraints are validated. Implement the after-story menu
unlock only after its milestone criteria and persistence requirements are
decided.

## Project-specific starting point

The current project is a small playable prototype with map-based gameplay,
CD audio, and a 3D model background. Its `makefile` enables the TGA, audio,
filesystem, and 3D modules. The project makefile remains the build entry point.

## PAL and NTSC support

The game should run as separate PAL or NTSC builds, selected at compile time.
The project makefile defaults to PAL and accepts `JO_NTSC=1` for NTSC; use
`make JO_NTSC=0 all` for PAL and `make JO_NTSC=1 all` for NTSC. Building the
other version rebuilds Jo Engine objects with the matching regional timing and
screen definitions. A single binary does not switch television standards at
runtime.

Jo Engine's default resolution is 320 x 256 in PAL and 320 x 240 in NTSC.
Gameplay layout, camera limits, HUD placement, and cutscene/UI safe areas must
be checked in both builds so the extra PAL height does not become a dependency.
VS Code provides matching PAL and NTSC IntelliSense configurations; select the
same configuration as the build being worked on.

The regional refresh rates also differ: PAL is 50 Hz and NTSC is 60 Hz.
Regional builds must preserve gameplay speed and timed behavior in real time;
see [Frame-rate-independent gameplay](#frame-rate-independent-gameplay). The
current prototype still uses frame-based Jo Engine physics and requires this
follow-up before its timing can be considered equivalent across regions.

See [JOENGINE_REFERENCE.md](./JOENGINE_REFERENCE.md) for verified engine and
project facts that inform this plan.
