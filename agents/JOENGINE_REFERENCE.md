# Jo Engine Reference for This Project

This document records project-specific facts verified against the Jo Engine
2026.3 checkout. Confirm API details against the local engine headers and
samples when implementing; do not assume every API from another Jo Engine
version is identical.

## Project starting point

- The project is `Projects/PD_wings` inside the Jo Engine checkout.
- The engine source and compiler folders are referenced by the project
  `makefile` as `../../jo_engine` and `../../Compiler`.
- `main.c` currently contains only Jo Engine startup, a TGA sprite load, and a
  draw callback.
- The project `makefile` has `JO_COMPILE_WITH_TGA_MODULE = 1`,
  `JO_COMPILE_WITH_AUDIO_MODULE = 0`, and `JO_COMPILE_WITH_3D_MODULE = 0`.
- The existing build lists `main.c` in `SRCS`; add any new C implementation
  files there.

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

- The project currently disables `JO_COMPILE_WITH_3D_MODULE`; set it to `1`
  when using Jo Engine's 3D APIs.
- The 3D module exposes camera and 3D drawing APIs. The `demo - 3D map` sample
  shows initializing a camera, setting its viewpoint, and drawing under a
  matrix push/pop.
- Test actual layer composition when combining 3D geometry with the 2D map,
  sprites, and foreground effects. A particular draw order or transparency
  outcome should not be assumed without a prototype.

Primary sources: `jo_engine/jo/3d.h`,
`Samples/demo - 3D map/main.c`, and `Samples/demo - 3D/main.c`.

## Audio and CD music

- The project currently disables `JO_COMPILE_WITH_AUDIO_MODULE`; enable it to
  use Jo Engine audio APIs.
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
