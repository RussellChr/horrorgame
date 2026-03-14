# Metamorph – Horror Game – Comprehensive Documentation

## Project Overview

**Metamorph** is a horror-themed narrative game developed in C using SDL3 for graphics rendering. 

---

## What is Currently Implemented

### Core Systems Completed:
- **Main Game Loop**: Event handling, update cycle, and rendering synchronized at ~60 FPS 
- **Game State Machine**: Seven distinct game states (Menu, Playing, Dialogue, Inventory, Pause, Ending, Quit) with complete state transitions
- **Player System**: 
  - Character
  - stats: Health (0–100), Sanity (0–100), Courage (0–100)
  - Inventory system
  - Animation system (File missing)
- **World/Location System**:
  - Main world
  - Interactable Objects/NPC
- **Dialogue System**:
  - Tree-based dialogue
  - Branching choices
  - Visual typewriter effect for dialogue text rendering
- **Story Progression**:
  - None
- **UI/Rendering**:
  - Main menu with three buttons (New Game, Load Game, Quit)
  - HUD overlay showing player stats (health, sanity, courage bars)(might change)
  - Interaction prompts near interactive zones
  - Inventory screen with item selection and description display
  - Pause menu
  - Ending screen with ending narrative text
  - 8×8 bitmap font rendering with text scaling and text wrapping
  - PNG background texture for dialogue (via SDL3_image) (Also might change)
- **Camera System**:
  - Smooth camera following of the player character
- **NPC System**:
  - NPC manager for storing multiple NPCs (up to 16)
  - Interaction detection based on distance from player
- **Collision Detection**:
  - Room Collision (Temporary)
- **Asset Loading**:
  - File-based world loading (locations.txt)
  - Story data loading
  - Dialogue tree population from file data
- **Animation System**:
  - Sprite frame animation with configurable frame counts and frame rates
  - Looping and non-looping animation support
  - Per-frame timer management

### Completed Features:
✅ Main menu and navigation  
✅ Player movement with WASD keys  
✅ Dialogue interactions with branching paths  
✅ Inventory system  
✅ Stat tracking 
✅ Collision detection and physical interaction  
✅ Camera system with smooth following  
✅ HUD and UI overlays  
✅ Animation system for characters  
✅ Pause menu  

---

## What is Incomplete

### Missing or Stubbed Features:
- **Save/Load System**: No persistent save data; game always starts fresh 
- **Audio System**: No sound effects or music; audio integration layer not implemented
- **Advanced NPC Behavior**: NPC system is defined but largely haven't been implemented
- **Additional Dialogue Trees**: Dialogue system exists but full dialogue content is incomplete
- **Additional Endings/Secret Paths**: None
- **Procedural Generation**: Atmospheric descriptions are static; no procedural generation of environment descriptions
- **More NPCs/Characters**: Only Main Character and a test NPC

---

## What is Planned Next

1. **Persistent Save/Load**: Binary or JSON-based save system for player progress
2. **More NPCs and Branching Dialogue Trees**: Expand NPC count and dialogue complexity
3. **Sound Cues via Audio Library**: Integrate audio library for ambient sound and music
4. **Procedurally Generated Atmospheric Descriptions**: Dynamic environmental storytelling
5. **Additional Endings and Secret Paths**: Expand ending branching and hidden story routes
6. **Quality of Life**: Performance optimization, expanded testing

---

## How to Compile

### Compiler Version
- **Minimum**: GCC 11 or Clang 12 (C11 standard)
- **Tested on**: GCC 13+, Clang 15+ (on macOS and Linux)
- **Windows Cross-Compilation**: MinGW-w64 with x86_64 target

### Required Libraries
1. **SDL3** (SDL3-3.x or later)
   - Core graphics and event handling library
   - Install via package manager or from source
   - On macOS (Homebrew): `brew install sdl3`
   - On Linux (Ubuntu): `sudo apt-get install libsdl3-dev`
   - On Windows: Download prebuilt binaries or compile from source
   
2. **SDL3_image** 
   - Enables PNG loading for dialogue background texture
   - On macOS (Homebrew): `brew install sdl3_image`
   - On Linux: `sudo apt-get install libsdl3-image-dev`
   - CMakeLists.txt automatically enables `HAVE_SDL3_IMAGE` define if found

3. **CMake** (3.10 or later)
   - Build system configuration tool
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

### Configuration Steps

1. **Ensure SDL3 is installed and discoverable:**
   - CMakeLists.txt includes `/usr/local` and `/opt/homebrew` in the prefix path for both Intel and Apple Silicon Homebrew installations
   - On non-standard installations, set `SDL3_DIR` environment variable:
     ```bash
     export SDL3_DIR=/path/to/SDL3/cmake
     cmake -B build ...
     ```

2. **For Windows MinGW builds:**
   - Ensure `mingw.cmake` toolchain file is in the repository root (already present)
   - Set appropriate MinGW binary path if not in `PATH`

3. **Optional SDL3_image configuration:**
   - If SDL3_image is not found, PNG loading is disabled automatically
   - To force enable/disable, pass `-DHAVE_SDL3_IMAGE=ON` or `-DHAVE_SDL3_IMAGE=OFF` to cmake

4. **Build output location:**
   - Executable is placed in `build/horrorgame` (or `build/horrorgame.exe` on Windows)

---

## How to Run

### How to Launch the Executable

**After compilation:**
```bash
./build/horrorgame              # Linux/macOS
./build/horrorgame.exe          # Windows
```

The game window will open with the title "Project Yozora – A Horror Story" at **1280×720 resolution** with a dark purple background.

### Controls

| Action | Input |
|--------|-------|
| **Move Left** | `A` |
| **Move Right** | `D` |
| **Move Up** | `W` |
| **Move Down** | `S` |
| **Interact** | `E` (when near interactive object; prompt shown) |
| **Open Inventory** | `I` |
| **Close Inventory** | `I` or `ESC` |
| **Pause** | `ESC` |
| **Menu Navigation** | `UP` / `DOWN` arrow keys |
| **Menu Select** | `ENTER` |
| **Dialogue Navigation** | `UP` / `DOWN` arrow keys (select choice) |
| **Dialogue Confirm** | `ENTER`, `SPACE`, or `E` (advance dialogue) |
| **Dialogue Exit** | `ESC` (skip current dialogue) |
| **Inventory Navigation** | `UP` / `DOWN` arrow keys |

### Game Flow
1. **Main Menu**: Select "New Game" to start (or "Quit" to exit)
2. **Gameplay**: Explore the Ashwood Estate using WASD
3. **Interaction**: Press `E` near NPCs or interactive objects to trigger dialogue
4. **Inventory**: Press `I` to view collected items (Candle, Diary, Key, etc.)
5. **Story Progression**: Player choices in dialogue affect stats and story flags, which determine the chapter progression and final ending
6. **Ending**: After story completion, an ending screen displays with narrative text

### Known Issues and Limitations

1. **No Persistent Save System**: Game progress is lost on exit; always starts from the beginning
2. **Limited NPC Implementation**: Only the Stranger NPC in the Entrance Hall has visible implementation; Lily exists in dialogue but has no physical world presence (visual representation)
3. **Static Atmosphere**: Environmental descriptions are hardcoded; no dynamic generation
4. **No Audio**: Horror atmosphere relies solely on visuals and narrative; no sound effects or music
5. **Performance**: Large dialogue trees or many simultaneous NPCs may cause minor frame rate dips
6. **Dialogue Tree Edge Cases**: 
   - Very long dialogue text may overflow the dialogue box (rendering clipped)
   - Nested conditionals (e.g., requiring both an item AND a courage threshold) not fully tested
7. **Collision Edge Cases**: Player can occasionally clip slightly into walls during fast diagonal movement
8. **Camera Smoothing**: Rapid movement changes can cause minor visual jitter (smooth follow has slight lag by design)
9. **Windows Compatibility**: Cross-compilation tested but runtime on Windows MinGW may require SDL3 DLLs in system PATH or local directory
10. **Platform-Specific Paths**: Asset file paths are hardcoded; different OSes may require path adjustments if assets folder is moved

---

## Project Structure Explanation

### Folder Structure

```
RussellChr/horrorgame/
├── CMakeLists.txt              Build configuration (defines compilation rules, links SDL3)
├── mingw.cmake                 MinGW cross-compilation toolchain
├── README.md                   Project overview (high-level)
├── .gitignore                  Git exclusion rules
├── docs/
│   └── DESIGN.md              Design document (narrative, story structure, endings, architecture)
├── include/                    Header files (declarations and interfaces)
│   ├── game.h                 Main game context and state machine
│   ├── player.h               Player struct, stats, inventory, animations
│   ├── world.h                Location graph, exits, decorations, colliders
│   ├── dialogue.h             Dialogue trees, nodes, choices, visual state
│   ├── story.h                Story progression, chapters, endings, flags
│   ├── ui.h                   UI components (buttons, HUD, stat bars)
│   ├── render.h               Graphics primitives, font rendering, textures
│   ├── camera.h               Camera system (viewport, coordinate conversion)
│   ├── animation.h            Animation frame management
│   ├── collision.h            AABB collision structures and functions
│   ├── utils.h                String, file, random, math utilities
│   └── npc.h                  NPC structures, behavior states, AI
├── src/                        C source files (implementations)
│   ├── main.c                 Entry point; SDL initialization, main loop
│   ├── game.c                 Core game logic; state machine, event dispatch
│   ├── player.c               Player stat modification, inventory, rendering
│   ├── world.c                Location graph, room rendering, file loading
│   ├── dialogue.c             Dialogue tree management, visual dialogue renderer
│   ├── story.c                Chapter progression, ending determination
│   ├── ui.c                   HUD drawing, button rendering, menus
│   ├── render.c               Bitmap font rendering, graphics primitives
│   ├── camera.c               Camera logic (smooth follow, coordinate transforms)
│   ├── animation.c            Animation frame updates and timing
│   ├── collision.c            AABB collision detection and response
│   ├── utils.c                String/file/random/math helpers
│   └── npc.c                  NPC creation, updates, rendering
├── assets/                     Game content files
│   ├── story.txt              Narrative text for chapters and endings
│   ├── locations.txt          Location graph definition (parsed by world.c)
│   └── dialogue.png (optional) Dialogue UI background texture
├── maps/                       Room/level data (future expansion)
│   └── rooms.txt              Room layout definitions
└── scripts/                    Game scripts and data
    └── dialogue.txt           Dialogue script data
```

### Main Modules and Responsibilities

| Module | Responsibility | Key Functions | Data Structures |
|--------|-----------------|-----------------|------------------|
| **main.c** | Entry point; SDL initialization; main loop setup | `main()` | None (calls game_init) |
| **game.c** | Core game state machine; event dispatch; frame orchestration | `game_init()`, `game_handle_event()`, `game_update()`, `game_render()`, `game_change_location()`, `game_start_dialogue()` | `Game` (main context) |
| **player.c** | Player character management; stats; inventory; animation | `player_create()`, `player_modify_*()`, `player_add_item()`, `player_set_flag()`, `player_update()`, `player_render()` | `Player`, `Item`, `Animation` |
| **world.c** | Location graph; room rendering; collision setup; file loading | `world_create()`, `world_load_locations()`, `world_setup_rooms()`, `world_render_room()`, `world_move()` | `World`, `Location`, `Exit`, `Decor`, `TriggerZone` |
| **dialogue.c** | Dialogue tree construction; visual dialogue playback; choice branching | `dialogue_tree_create()`, `dialogue_add_node()`, `dialogue_state_update()`, `dialogue_state_advance()`, `dialogue_render()` | `DialogueTree`, `DialogueNode`, `DialogueChoice`, `DialogueState` |
| **story.c** | Story progression; chapter transitions; ending determination | `story_create()`, `story_advance_chapter()`, `story_determine_ending()`, `story_trigger_event()` | `StoryState`, `Ending` enum |
| **ui.c** | HUD rendering; button logic; stat bar display | `ui_draw_hud()`, `ui_draw_stat_bar()`, `draw_button()`, `ui_draw_interact_prompt()` | `Button` |
| **render.c** | Graphics primitives; bitmap font rendering; texture loading | `render_filled_rect()`, `render_text()`, `render_text_wrapped()`, `render_load_texture()` | 8×8 font bitmap data |
| **camera.c** | Camera viewport logic; smooth following; coordinate transforms | `camera_init()`, `camera_follow()`, `camera_snap()`, `camera_to_screen_x()`, `camera_to_world_x()` | `Camera` |
| **animation.c** | Frame-based animation timing and state | `animation_init()`, `animation_update()`, `animation_get_frame()` | `Animation` |
| **collision.c** | AABB collision detection and response | `rect_overlaps()`, `rect_resolve_collision()` | `Rect`, `TriggerZone` |
| **npc.c** | NPC lifecycle; AI updates; rendering | `npc_create()`, `npc_manager_create()`, `npc_update()`, `npc_render()` | `NPC`, `NPCManager` |
| **utils.c** | String/file utilities; random generation; math helpers | `utils_strdup()`, `utils_read_file()`, `utils_rand_range()`, `utils_clamp()` | None (utility functions only) |

