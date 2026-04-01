# Project Yozora – Game Design Document

## Overview

**Project Yozora** is a 2D visual-novel horror game written in C using SDL3.
Players explore a mysterious facility, encounter unsettling characters, and
experience the prologue of a larger story.

The game runs in a 1280×720 window with sprite-based movement, tile-map
collision, an inner-monologue dialogue system, and atmospheric room rendering.

---

## Core Pillars

1. **Atmosphere over action** – suspense is built through description and pacing,
   not combat.
2. **Meaningful choices** – dialogue decisions change flags that shape future events.
3. **Low-resource horror** – minimal assets prove that horror does not require
   large production budgets.

---

## Locations (Prologue)

| ID | Name          | Map CSV                          | Background Texture        |
|----|---------------|----------------------------------|---------------------------|
| 0  | Entrance Hall | archive_close.csv                | assets/room/archive_room.png |
| 1  | Kimia Lab     | lab.csv                          | assets/room/lab.png       |
| 2  | Hallway       | hallway.csv                      | assets/room/hallway.png   |

The player starts in the Hallway (location 2). Rooms 0 and 1 are connected
via door triggers built from the tile maps.

---

## Interactions (Prologue)

| Location | Trigger ID | Event                              |
|----------|------------|------------------------------------|
| 0        | 30         | Portrait monologue                 |
| 0        | 40         | Stranger NPC dialogue              |

---

## Dialogue System

Conversations use a tree of `DialogueNode` objects. Each node has a speaker,
text, and up to `MAX_CHOICES` (8) `DialogueChoice` entries.

Inner monologues are loaded from `assets/dialogue/monologues.txt` and
converted to dialogue trees at runtime. Active sections: `game_start`,
`portrait`, `stranger`.

---

## Architecture

```
src/
  main.c       – Entry point; SDL3 init; event loop
  game.c       – Game loop; state machine; input dispatch; rendering
  player.c     – Player struct; movement; inventory; animation
  world.c      – Location graph; tile-map loading; room rendering
  dialogue.c   – Dialogue tree; visual conversation runner
  story.c      – Story state; prologue intro text
  monologue.c  – Inner monologue loader/parser
  npc.c        – NPC system
  ui.c         – Buttons; sliders; HUD; interaction prompts
  render.c     – Bitmap font; primitives; texture helpers
  camera.c     – Camera follow and snap
  collision.c  – AABB overlap and resolution
  animation.c  – Frame-based animation state
  map.c        – CSV tile-map loader and collider builder
  utils.c      – String/file helpers; slow-print; clamp

include/
  game.h / player.h / world.h / dialogue.h / story.h
  monologue.h / npc.h / render.h / camera.h / collision.h
  animation.h / ui.h / map.h / utils.h

assets/
  dialogue/monologues.txt  – Inner monologue sections
  player/                  – Player sprite sheets (idle + walk, 4 directions)
  room/                    – Room background textures
  dialogue.png             – Dialogue box background
  title_screen.png         – Title screen image
  locations.txt            – Location metadata (name, description, atmosphere)

maps/
  hallway.csv                    – Hallway tile map
  archive_close.csv              – Entrance Hall tile map (closed state)
  archive_open.csv               – Entrance Hall tile map (open state)
  lab.csv                        – Kimia Lab tile map
  rooms.txt                      – Room layout reference

docs/
  DESIGN.md    – This document
```

---

## Build

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/usr/local -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/horrorgame
```

Optional flags passed to the executable:

| Flag         | Meaning                              |
|--------------|--------------------------------------|
| `-n <name>`  | Set the player character name        |
| `-a <path>`  | Override the assets directory path   |

---

## Controls

| Key          | Action                        |
|--------------|-------------------------------|
| WASD / Arrows| Move player                   |
| E            | Interact                      |
| I            | Open inventory                |
| ESC          | Pause                         |
| ENTER/SPACE  | Advance dialogue              |

---

## Future Work

- Chapter 1 and beyond: expand the story past the prologue
- Persistent save/load
- More NPCs and branching dialogue trees
- Sound cues via a simple audio library
