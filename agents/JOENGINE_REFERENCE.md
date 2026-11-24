# Jo Engine Reference for This Project

This document records project-specific facts verified against the Jo Engine
2026.3 checkout. Confirm API details against the local engine headers and
samples when implementing; do not assume every API from another Jo Engine
version is identical.

## Project starting point

- The project is `Projects/PD_wings` inside the Jo Engine checkout.
- The engine source and compiler folders are referenced by the project
  `makefile` as `../../jo_engine` and `../../Compiler`.
- `main.c` initializes Jo Engine and starts the game. The `src/` directory
  contains `game.c` for loop/setup coordination, `input.c` for input,
  `player.c` for player behavior, and `level.c` for level loading/drawing.
  Shared runtime state is declared in `src/game_state.h`.
- The project `makefile` has `JO_COMPILE_WITH_TGA_MODULE = 1`,
  `JO_COMPILE_WITH_AUDIO_MODULE = 1`, `JO_COMPILE_WITH_FS_MODULE = 1`,
  `JO_COMPILE_WITH_PROFILER_MODULE = 1`,
  `JO_COMPILE_WITH_SPRITE_HASHTABLE = 1`, and
  `JO_COMPILE_WITH_3D_MODULE = 1`.
- Press B to toggle the performance overlay. It shows Jo Engine profiler
  timings plus FPS, late frames, dynamic memory usage, and fragmentation.
  `src/profiler.c` owns the overlay, and `game.c` times the game update and
  draw phases with user profiler slots.
- The project defaults to PAL (`JO_NTSC = 0`); build NTSC with
  `make JO_NTSC=1 all`. Jo Engine defaults to 320 x 256 in PAL and
  320 x 240 in NTSC when screen dimensions are not explicitly set.
- The project boot header retains the engine default area code `JTUE`, which
  supports Europe as well as Japan, Taiwan, and the USA. The shared build
  supports overriding this with `JO_GAME_AREA_CODE`.
- With the default `JO_FRAMERATE = 1`, the game loop runs once per VBlank:
  PAL provides 50 updates per second and NTSC 60. Physics and timers advanced
  by fixed per-callback amounts are therefore not equivalent in real time;
  the current test movement is frame-based.
- `.vscode/c_cpp_properties.json` has separate PAL and NTSC configurations;
  select the configuration matching the active build to keep platform macros
  accurate in IntelliSense.
- CD audio files are read from `audio/`; the current `track1.wav` is CD audio
  track 2 because track 1 contains game data.
- `JO_CD_SECTOR_SIZE = 2352` builds a combined `game.bin` disc image so the cue
  sheet references that single file instead of the nested `audio/track1.wav`
  path rejected by Mednafen's default path security check.
- The project `makefile` explicitly lists the application modules in `SRCS`
  and adds `src/` to the include path; add each new C implementation file
  there.
- VS Code C/C++ IntelliSense paths and Jo Engine feature defines are configured
  in `.vscode/c_cpp_properties.json`; keep it aligned with the project makefile.

## Current level assets and controls

- The active graphics map is `cd/LVL/ZONE1/MAP/Z1A1G.MAP`; its sprites are listed
  in `cd/LVL/ZONE1/TEX/Z1A1.TEX` and loaded from `cd/LVL/ZONE1/TEX/`. The
  separate collision map is `cd/LVL/ZONE1/MAP/Z1A1C.MAP`; its mask sprites are
  listed in `cd/LVL/ZONE1/TEX/Z1C.TEX`. The graphics map is drawn, while the
  collision map is only queried. The player
  starts near map coordinates (100, 250); the map offsets start at (0, 0).
  The sky uses the VDP2 back-screen color. Map tiles draw at depth 300 and the
  player at depth 250; 3D scenery should use a depth greater than 300 to stay
  behind the map.
- `jo_fs_cd()` and the file-loading `sub_dir` arguments accept one directory
  component, not a slash-separated path. Navigate one directory at a time
  (`LVL`, then `ZONE1`), pass the leaf directory (`TEX` or `MAP`) to the
  loader, then return with `JO_PARENT_DIR`. Character sheets are loaded by
  navigating into each directory under `CHAR` and passing `JO_CURRENT_DIR`.
- REI is the first playable character. Its `IDLE.BIN` contains fifteen
  48 x 48 frames and `RUN.BIN` contains eight 48 x 48 frames. SAM is the
  second playable character; its `IDLE.BIN` contains ten 48 x 48 frames and
  `RUN.BIN` contains eight 48 x 48 frames. The old TEST spritesheet is no
  longer loaded by the game.
- REI and SAM share the current movement/controller logic, but use their own
  animation sheets and max-speed tuning. X switches between them.
- Player ground collision uses `jo_map_per_pixel_vertical_collision()` at the
  visible feet and Jo's Sonic-style gravity/jump helpers. Ground and slope
  response comes from collision-mask pixels rather than map attributes.
  Grounded movement probes the center-foot position at a short
  movement-direction lookahead and scans up to eight pixels upward to step
  onto rising slopes across tile seams; it similarly projects down up to eight
  pixels to keep grounded movement attached to descending slopes.
  Horizontal movement checks the leading body edge against collision-mask
  pixels one map pixel at a time. Wall probes omit the bottom 16 pixels of the
  body so slope contact is handled by ground projection rather than a wall hit.
- Horizontal camera movement retains fractional speed between frames and
  applies whole-pixel scroll steps, avoiding forced one-pixel movement at
  subpixel speeds.
- D-pad moves, A jumps, X switches between REI and SAM, B toggles performance
  diagnostics, and START resets the level.
- The game starts CD track 2 on repeat after loading CD files, avoiding
  loading assets from the drive while music is already playing.

## Core loop and callbacks

- Include Jo Engine through `#include <jo/jo.h>`.
- Call `jo_core_init()` first in `jo_main()`.
- Register normal game-loop callbacks with `jo_core_add_callback()` before
  `jo_core_run()`. The engine header describes these as callbacks in the main
  CPU game loop.
- `jo_core_run()` is the last call in `jo_main()`.
- `jo_core_add_callback()` returns an event ID and
  `jo_core_remove_callback()` can remove it. A persistent dispatcher callback
  is a suitable starting pattern for a state-driven game; ordinary state
  changes need not imply callback registration changes.
- Draw and update callback ordering/timing should be verified in a prototype
  rather than inferred from API names.

Primary source: `jo_engine/jo/core.h`.

## JoMapEditor and sprite maps

- JoMapEditor creates Jo Engine `.MAP` files. Its README describes the map as
  sprite placements with Z-index; it supports tile alignment and multi-select.
- The map file references image filenames and their coordinates; the map API
  also supports an optional user attribute for a tile. The engine sample
  demonstrates loading a texture image pack before loading the map.
- Load a `.MAP` with
  `jo_map_load_from_file(layer, depth, sub_dir, filename)`. It returns `bool`;
  the header recommends `jo_get_last_error()` for load failures.
- Draw a 2D map with `jo_map_draw(layer, screen_x, screen_y)`.
- `jo_map_per_pixel_vertical_collision()` checks vertical distance to map
  surfaces. `jo_map_hitbox_detection_custom_boundaries()` checks a rectangular
  hitbox and returns the colliding tile attribute or `JO_MAP_NO_COLLISION`.
- The collision helper coordinates are screen-relative. Keep conversion
  between player/world coordinates and map/camera offsets consistent.
- These are sprite-map collision tools, not a general-purpose physics engine.
  Collision behavior depends on the map art/attributes and should be tested
  with the actual level assets.

Primary sources: `jo_engine/jo/map.h`, `Samples/demo - map/main.c`,
`Samples/demo - sonic/main.c`, and `JoMapEditor/README.txt`.

## 3D support

- The project enables `JO_COMPILE_WITH_3D_MODULE` and the matching
  `JO_COMPILE_WITH_3D_SUPPORT` IntelliSense define.
- The 3D module exposes camera and 3D drawing APIs. The `demo - 3D map` sample
  shows initializing a camera, setting its viewpoint, and drawing under a
  matrix push/pop.
- `jo_3d_load_model()` reads a `.JOM` model from the current CD directory.
  Models may embed textures; this test model contains its texture data.
- Test actual layer composition when combining 3D geometry with the 2D map,
  sprites, and foreground effects. A particular draw order or transparency
  outcome should not be assumed without a prototype.

Primary sources: `jo_engine/jo/3d.h`,
`Samples/demo - 3D map/main.c`, and `Samples/demo - 3D/main.c`.

## Audio and CD music

- The project enables `JO_COMPILE_WITH_AUDIO_MODULE` to use Jo Engine audio
  APIs.
- CD-track playback APIs are declared under
  `JO_COMPILE_WITH_FS_SUPPORT`. Explicitly enable
  `JO_COMPILE_WITH_FS_MODULE = 1` for a project that loads map/assets from the
  CD and uses CD music. The common makefile also documents automatic FS
  enabling for some file-reading configurations, but an explicit setting makes
  this project requirement clear.
- `jo_audio_play_cd_track(from_track, to_track, repeat)` selects tracks;
  track 1 is the game data and music tracks start at 2.
- The common makefile documents audio tracks as files in
  `JO_CD_AUDIO_DIR` (the game folder by default), ordered alphabetically
  without regard to case. The build generates `game.cue`; use that file to
  verify the intended track numbers.
- For a CD audio WAV, the documented standard is PCM, 44100 Hz, 16-bit,
  stereo. The engine header notes that MP3 support depends on emulator and
  disc-image arrangement; prefer correctly formatted WAV for the Saturn
  project.
- CD audio and file loading use the same drive. The audio API notes that
  loading files while CD audio is paused is supported; design level loading
  around that constraint.
- `jo_audio_is_cd_playing()`, `jo_audio_pause_cd()`,
  `jo_audio_resume_cd()`, and `jo_audio_stop_cd()` are available for managing
  playback.

Primary sources: `jo_engine/jo/audio.h`,
`Compiler/COMMON/jo_engine_makefile`,
`Compiler/COMMON/JoEngineCueMaker.c`, and `Samples/demo - audio/main.c`.

## Relevant local examples

- `Samples/demo - map/main.c` — sprite map loading, drawing, and collision
  helper usage.
- `Samples/demo - sonic/main.c` and `sonic.c` — a compact side-scroller sample
  using map collision, movement, and camera scrolling.
- `Samples/demo - 3D map/main.c` — camera setup and 3D map drawing.
- `Samples/demo - audio/main.c` — CD track playback and pause/resume.
- `Samples/demo - 2D storyboard/main.c` — a callback-based game loop and
  sprite animation setup.
