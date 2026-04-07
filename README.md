# Project Yozora – Horror Game – Comprehensive Documentation

## Project Overview

**Project Yozora** is a horror-themed narrative game developed in C using SDL3 for graphics rendering. The game follows a character waking up with no memory in a facility, exploring multiple rooms while evading a patrolling enemy.

---

## What is Currently Implemented

### Core Systems Completed:
- **Main Game Loop**: Event handling, update cycle, and rendering synchronized at ~60 FPS
- **Game State Machine**: Eleven distinct game states with complete transitions:
  - Menu, Playing, Dialogue, Inventory, Pause, Settings, Locker, Simon Says, Jumpscare, Game Over, Quit
- **Player System**:
  - Character movement at 220 px/s with WASD
  - Directional sprite animations (N/S/E/W idle + walk, 2 walk frames per direction)
  - Inventory system (up to 16 items)
  - Story flag bit-field for progression tracking
- **World/Location System**:
  - Seven distinct rooms: Archive, Hallway, Hibernation Lab, Kimia Lab, Monitoring Room, Power Room, Security Room
  - CSV tile-map loading for collision and door triggers in each room
  - Room background art for every location
  - Interactable trigger zones per room
- **Dialogue & Monologue System**:
  - Tree-based dialogue with branching choices
  - Visual typewriter effect for dialogue text
  - Inner monologue system loaded from `assets/dialogue/monologues.txt`; long text is auto-split into chained nodes
- **Enemy System**:
  - A\* pathfinding on the hallway tile grid
  - Two-state AI: patrol (waypoints) and chase (triggered within 350 px)
  - Directional sprite animations (forward, backward, left, right) with frame caching across direction changes
  - Hysteresis-based direction selection to reduce diagonal flicker
- **Locker Hiding Mechanic**:
  - Player can hide in a locker to evade the enemy
  - Breathing minigame (timed SPACE presses; 5 successes required)
  - Locker view shows hallway minimap with enemy and player markers
  - Failing the minigame triggers game over immediately
- **Simon Says Minigame**:
  - 10-round sequence; sequences grow each round
  - Round 7 completion triggers a jumpscare
- **Jumpscare**:
  - Full-screen video playback via FFmpeg (`jumpscare.mov`)
  - Transitions to game over after playback
- **Visual Effects**:
  - Flashlight beam (warm-yellow cone, additive blend) in the player's facing direction
  - Archive room darkness (light-mask via `SDL_BLENDMODE_MOD`) with rare ambient flicker pulses
  - Gas mask circular vignette overlay
  - Ambient screen vignette
- **Security / Monitor System**:
  - Zoomable security monitor view
  - Passcode entry overlay (4-digit, correct code: 3643)
  - Containment level sub-screen
  - AM recording audio playback via `SDL_AudioStream`
- **Item System**:
  - Flashlight (toggleable beam), Power Cell, Gas Mask (toggleable vignette), Keycard, Fuel Tank
  - Item pickup notification banner
  - Inventory icons (PNG textures per item)
- **UI/Rendering**:
  - Title screen texture on main menu
  - Settings menu with volume and brightness sliders
  - 8×8 bitmap font with text scaling and wrapping
  - HUD interaction prompts near trigger zones
  - Pause menu
- **Camera System**: Smooth camera following with world↔screen coordinate conversion
- **NPC System**: NPC manager supporting up to 16 NPCs with proximity-based interaction
- **Collision Detection**: CSV-derived AABB colliders; adjacent wall tiles merged into single rects
- **Audio**: SDL3 audio stream for AM recording WAV playback
- **Video**: FFmpeg-backed `VideoPlayer` for jumpscare MOV decoding and rendering

### Completed Features:
✅ Main menu with title screen  
✅ Player movement (WASD) with directional sprite animations  
✅ Dialogue and inner monologue system  
✅ Inventory system with item icons  
✅ Story flag tracking  
✅ CSV tile-map collision and door triggers  
✅ Camera system with smooth following  
✅ HUD interaction prompts  
✅ Pause menu and settings menu  
✅ Enemy AI (A\* pathfinding, patrol/chase, directional animations)  
✅ Locker hiding mechanic with breathing minigame  
✅ Simon Says minigame  
✅ Jumpscare video playback  
✅ Flashlight, gas mask, and archive darkness visual effects  
✅ Security monitor with passcode entry and AM audio playback  
✅ Seven rooms with individual room art and tile maps  

---

## What is Incomplete

### Missing or Stubbed Features:
- **Save/Load System**: No persistent save data; game always starts fresh
- **Full Dialogue Content**: Dialogue system and monologue file exist but story content is sparse
- **Additional Endings/Secret Paths**: Ending branching logic is stubbed
- **More NPCs**: Only the player character and a test NPC are implemented
- **Ambient Audio**: Background music and general sound effects are not yet integrated

---

## What is Planned Next

1. **Persistent Save/Load**: Binary or JSON-based save system for player progress
2. **Full Monologue and Dialogue Trees**: Complete story content for all rooms and NPCs
3. **Ambient Sound and Music**: Integrate audio library for background atmosphere
4. **Additional Endings and Secret Paths**: Expand ending branching and hidden story routes
5. **Quality of Life**: Performance optimisation, additional testing

---

## How to Compile

### Compiler Version
- **Minimum**: GCC 11 or Clang 12 (C11 standard)
- **Tested on**: GCC 13+, Clang 15+ (on macOS and Linux)
- **Windows Cross-Compilation**: MinGW-w64 with x86_64 target

### Required Libraries
1. **SDL3** (SDL3-3.x or later)
   - Core graphics and event handling library
   - On macOS (Homebrew): `brew install sdl3`
   - On Linux: `sudo apt-get install libsdl3-dev`
   - On Windows: Download prebuilt binaries or compile from source

2. **SDL3_image** *(optional)*
   - Enables PNG loading for room backgrounds and item icons
   - On macOS (Homebrew): `brew install sdl3_image`
   - On Linux: `sudo apt-get install libsdl3-image-dev`
   - Automatically detected; `HAVE_SDL3_IMAGE` is defined if found

3. **FFmpeg** *(required – for video playback)*
   - Libraries needed: `libavformat`, `libavcodec`, `libavutil`, `libswscale`, `libswresample`
   - On macOS (Homebrew): `brew install ffmpeg`
   - On Linux: `sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev`

4. **CMake** (3.10 or later)
   - `sudo apt-get install cmake` (Linux) or `brew install cmake` (macOS)

### Exact Compile Command

**Linux or macOS (native build):**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/horrorgame
```

**Windows (cross-compilation from macOS/Linux with MinGW):**
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=mingw.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/horrorgame.exe
```

**Debug build (with symbols):**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/horrorgame
```

### Configuration Notes

1. **SDL3 discovery**: CMakeLists.txt includes `/usr/local` and `/opt/homebrew` in the prefix path for both Intel and Apple Silicon Homebrew. For non-standard installs:
   ```bash
   export SDL3_DIR=/path/to/SDL3/cmake
   cmake -B build ...
   ```

2. **FFmpeg discovery**: Detected via `pkg-config`. Homebrew paths for macOS are automatically added. If pkg-config cannot find FFmpeg, set `PKG_CONFIG_PATH` accordingly.

3. **For Windows MinGW builds**: Ensure `mingw.cmake` is in the repository root (already present) and MinGW binaries are in `PATH`.

4. **Build output**: Executable is placed in `build/horrorgame` (or `build/horrorgame.exe`). The `assets/` and `maps/` folders are copied there automatically after each build.

---

## How to Run

### Launch the Executable

```bash
./build/horrorgame              # Linux/macOS
./build/horrorgame.exe          # Windows
```

### Controls

| Action | Input |
|--------|-------|
| **Move** | `W` / `A` / `S` / `D` |
| **Interact** | `E` (when near interactive object; prompt shown) |
| **Toggle Flashlight** | `F` |
| **Toggle Gas Mask** | `G` |
| **Open Inventory** | `I` |
| **Close Inventory** | `I` or `ESC` |
| **Pause / Back** | `ESC` |
| **Menu Navigation** | `UP` / `DOWN` arrow keys |
| **Menu Select** | `ENTER` |
| **Dialogue Navigation** | `UP` / `DOWN` arrow keys (select choice) |
| **Dialogue Confirm** | `ENTER`, `SPACE`, or `E` |
| **Dialogue Exit** | `ESC` |
| **Inventory Navigation** | `UP` / `DOWN` arrow keys |
| **Locker Breathing Minigame** | `SPACE` (timed presses) |

### Game Flow
1. **Main Menu**: Select "New Game" to start
2. **Gameplay**: Explore the facility rooms using WASD; read inner monologues at interactive points
3. **Enemy**: The enemy patrols the hallway and chases the player when nearby — hide in a locker to survive
4. **Items**: Pick up items (flashlight, gas mask, keycard, etc.) and use them from the inventory
5. **Security Room**: Examine the monitor, enter the passcode (`3643`), and play back the AM recording
6. **Minigames**: Complete Simon Says in the security room; survive the locker breathing minigame
7. **Jumpscare / Game Over**: Failing a minigame or being caught ends the game

### Known Issues and Limitations

1. **No Persistent Save System**: Progress is lost on exit; game always starts fresh
2. **Sparse Story Content**: Inner monologue and dialogue text is not fully written
3. **No Ambient Audio**: Background music and general sound effects are absent
4. **Dialogue Box Overflow**: Very long text may overflow the dialogue box
5. **Limited NPC Implementation**: Only a test NPC has a visible dialogue implementation

---

## Project Structure Explanation

### Folder Structure

```
RussellChr/horrorgame/
├── CMakeLists.txt              Build configuration (SDL3, FFmpeg, MinGW support)
├── mingw.cmake                 MinGW cross-compilation toolchain
├── README.md                   This file
├── .gitignore                  Git exclusion rules
├── docs/
│   └── DESIGN.md              Design document (narrative, story structure, architecture)
├── include/                    Header files
│   ├── game.h                 Main game context and state machine (11 states)
│   ├── player.h               Player struct, inventory, directional sprites
│   ├── world.h                Location graph, exits, decorations, colliders
│   ├── dialogue.h             Dialogue trees, nodes, choices, visual state
│   ├── monologue.h            Inner monologue sections loaded from file
│   ├── story.h                Story progression, chapters, endings, flags
│   ├── ui.h                   UI components (buttons, HUD, sliders)
│   ├── render.h               Graphics primitives, font rendering, textures
│   ├── camera.h               Camera system (viewport, coordinate conversion)
│   ├── animation.h            Sprite frame animation management
│   ├── collision.h            AABB collision structures and functions
│   ├── map.h                  CSV tile-map loading and collider/trigger building
│   ├── enemy.h                Enemy struct, A* pathfinding, patrol/chase AI
│   ├── effects.h              Visual effect renderers (flashlight, darkness, vignette)
│   ├── interactions.h         In-world interaction dispatcher
│   ├── video.h                FFmpeg-backed video player
│   ├── npc.h                  NPC structures, behavior, rendering
│   └── utils.h                String, file, random, math utilities
├── src/                        C source files
│   ├── main.c                 Entry point; SDL + FFmpeg initialisation, main loop
│   ├── game.c                 Core state machine; event dispatch; update logic
│   ├── game_render.c          Per-state render helpers (locker minimap, etc.)
│   ├── effects.c              Flashlight beam, archive darkness, gas mask vignette
│   ├── interactions.c         Trigger/interaction handler; dialogue tree builder
│   ├── player.c               Player movement, inventory, directional sprite render
│   ├── world.c                Location graph, room rendering, CSV tile setup
│   ├── map.c                  CSV map loader; collider and door-trigger builder
│   ├── dialogue.c             Dialogue tree management; typewriter renderer
│   ├── monologue.c            Monologue file parser; DialogueTree builder
│   ├── story.c                Chapter progression; ending determination
│   ├── ui.c                   HUD, buttons, sliders, stat bars
│   ├── render.c               8×8 bitmap font; graphics primitives; texture loader
│   ├── camera.c               Smooth camera follow; coordinate transforms
│   ├── animation.c            Frame timing and state
│   ├── collision.c            AABB detection and resolution
│   ├── enemy.c                A* pathfinding; patrol/chase AI; directional sprites
│   ├── video.c                FFmpeg video decoder + SDL3 audio stream output
│   ├── npc.c                  NPC lifecycle, updates, rendering
│   └── utils.c                String/file/random/math helpers
├── assets/                     Game content
│   ├── locations.txt          Location graph definition (parsed by world.c)
│   ├── title_screen.png       Main menu background
│   ├── dialogue.png           Dialogue UI background texture (optional)
│   ├── locker.png             Locker hiding view background
│   ├── note_locker.png        Security note shown inside locker
│   ├── monitor_zoom.png       Security monitor zoomed view
│   ├── containment_level.png  Containment level sub-screen
│   ├── flashlight.png         Flashlight inventory icon
│   ├── gasmask.png            Gas mask inventory icon
│   ├── keycard.png            Keycard inventory icon
│   ├── AM.wav                 AM recording audio clip
│   ├── jumpscare.mov          Jumpscare video (played after Simon Says round 7)
│   ├── dialogue/
│   │   └── monologues.txt     Inner monologue scripts (keyed by trigger ID)
│   ├── enemy/                 Enemy directional sprite frames
│   │   ├── forward/           Forward-facing frames (up to 8)
│   │   ├── backward/          Backward-facing frames (up to 6)
│   │   ├── left/              Left-facing frames (up to 8)
│   │   └── right/             Right-facing frames (up to 6)
│   ├── player/                Player directional sprites
│   │   ├── player_idle_{north,south,east,west}.png
│   │   └── player_walk_{north,south,east,west}_{0,1}.png
│   └── room/                  Room background art
│       ├── archive_room.png
│       ├── hallway.png
│       ├── hibernation.png
│       ├── lab.png
│       ├── monitoring_room.png
│       ├── power.png
│       └── security.png
└── maps/                       CSV tile maps (walls, floors, doors)
    ├── hallway.csv
    ├── archive_close.csv
    ├── archive_open.csv
    ├── hibernation.csv
    ├── lab.csv
    ├── power.csv
    ├── security.csv
    └── rooms.txt              Room layout metadata
```

### Main Modules and Responsibilities

| Module | Responsibility | Key Functions | Data Structures |
|--------|-----------------|-----------------|------------------|
| **main.c** | Entry point; SDL + FFmpeg initialisation; main loop | `main()` | — |
| **game.c** | State machine; event dispatch; update orchestration | `game_init()`, `game_handle_event()`, `game_update()`, `game_change_location()`, `game_start_dialogue()`, `game_start_simon()` | `Game` |
| **game_render.c** | Per-state render helpers (playing, locker, Simon, game over, …) | `game_render_playing()`, `game_render_locker()`, `game_render_simon()`, `game_render_game_over()` | — |
| **effects.c** | Visual overlay renderers | `render_flashlight_beam()`, `render_archive_darkness()`, `render_gasmask_vignette()`, `render_screen_vignette()` | — |
| **interactions.c** | In-world trigger/interaction dispatcher | `game_handle_interaction()`, `game_set_dialogue_tree()`, `game_apply_dialogue_choice_flag()` | — |
| **player.c** | Movement, inventory, directional sprite render | `player_create()`, `player_add_item()`, `player_set_flag()`, `player_update()`, `player_render()` | `Player`, `Item` |
| **world.c** | Location graph; room rendering; CSV tile setup | `world_create()`, `world_load_locations()`, `world_setup_rooms()`, `world_render_room()` | `World`, `Location`, `TriggerZone` |
| **map.c** | CSV tile-map loader; collider and trigger builder | `map_load_csv()`, `map_build_colliders()`, `map_build_door_triggers()`, `map_find_spawn()` | `Map` |
| **enemy.c** | A\* pathfinding; patrol/chase AI; directional animations | `enemy_init()`, `enemy_update()`, `enemy_render()`, `enemy_hits_player()` | `Enemy`, `EnemyState`, `EnemyDirection` |
| **dialogue.c** | Dialogue tree; typewriter renderer; choice branching | `dialogue_tree_create()`, `dialogue_add_node()`, `dialogue_state_update()`, `dialogue_render()` | `DialogueTree`, `DialogueNode`, `DialogueState` |
| **monologue.c** | Monologue file parser; builds `DialogueTree` from sections | `monologue_load()`, `monologue_find()`, `monologue_to_dialogue_tree()` | `MonologueFile`, `MonologueSection` |
| **story.c** | Chapter progression; ending determination | `story_create()`, `story_advance_chapter()`, `story_determine_ending()` | `StoryState` |
| **video.c** | FFmpeg video decoder; SDL3 audio stream playback | `video_player_open()`, `video_player_update()`, `video_player_render()`, `video_player_is_done()`, `video_player_close()` | `VideoPlayer` |
| **ui.c** | HUD; buttons; sliders; stat bars | `ui_draw_hud()`, `draw_button()`, `ui_draw_interact_prompt()` | `Button`, `Slider` |
| **render.c** | 8×8 bitmap font; graphics primitives; texture loader | `render_text()`, `render_text_wrapped()`, `render_load_texture()` | font bitmap data |
| **camera.c** | Smooth follow; coordinate transforms | `camera_init()`, `camera_follow()`, `camera_snap()`, `camera_to_screen_x()` | `Camera` |
| **animation.c** | Frame timing and state | `animation_init()`, `animation_update()`, `animation_get_frame()` | `Animation` |
| **collision.c** | AABB detection and resolution | `rect_overlaps()`, `rect_resolve_collision()` | `Rect` |
| **npc.c** | NPC lifecycle; updates; rendering | `npc_create()`, `npc_manager_create()`, `npc_update()`, `npc_render()` | `NPC`, `NPCManager` |
| **utils.c** | String/file/random/math helpers | `utils_strdup()`, `utils_read_file()`, `utils_rand_range()`, `utils_clamp()` | — |

