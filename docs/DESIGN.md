# Paper Lily – Game Design Document

## Overview

**Paper Lily** is a text-based survival horror game written in C, inspired
by the visual-novel horror game of the same name. Players explore a haunted
estate, uncover a dark secret, build a relationship with a trapped child
named Lily, and choose one of four story endings.

The game runs entirely in a terminal, using text descriptions, ANSI colour,
and typewriter-style output to build atmosphere.

---

## Core Pillars

1. **Atmosphere over action** – suspense is built through description and pacing,
   not combat.
2. **Meaningful choices** – every decision changes stats and flags that alter
   the available endings.
3. **Character empathy** – the player's relationship with Lily is the emotional
   heart of the game.
4. **Low-resource horror** – text-only presentation proves that horror does not
   require graphics.

---

## Player Stats

| Stat    | Range | Effect                                                   |
|---------|-------|----------------------------------------------------------|
| Health  | 0–100 | Reaches 0 → game over (physical death)                  |
| Sanity  | 0–100 | Reaches 0 → game over (madness); gates some choices     |
| Courage | 0–100 | Unlocks brave dialogue options; required for sacrifice   |

---

## Story Flags (bit positions in Player.flags)

| Bit | Name                   | Set when                                      |
|-----|------------------------|-----------------------------------------------|
| 0   | FLAG_MET_LILY          | First conversation with Lily occurs           |
| 1   | FLAG_FOUND_DIARY       | Player picks up the diary                     |
| 2   | FLAG_OPENED_BASEMENT   | Basement door is unlocked                     |
| 3   | FLAG_SOLVED_PUZZLE     | Ritual circle puzzle is completed             |
| 4   | FLAG_KNOWS_TRUTH       | Player reads the full diary + ritual book     |
| 5   | FLAG_LILY_TRUSTS_PLAYER| Lily's trust branch selected in dialogue      |
| 6   | FLAG_MONSTER_AWARE     | Player has learned the creature's nature      |
| 7   | FLAG_KEY_OBTAINED      | Basement key is in inventory                  |

---

## Locations

```
[Entrance Hall] ──north──► [Dark Corridor] ──north(locked)──► [Basement]
        │                                                          │
       east                                                       east
        │                                                          │
        ▼                                                          ▼
   [The Library] ──north──► [Child's Room]              [Ritual Room]
```

| ID | Name            | Danger | Notable Item        |
|----|-----------------|--------|---------------------|
| 0  | Entrance Hall   | No     | –                   |
| 1  | Dark Corridor   | Yes    | –                   |
| 2  | The Library     | No     | Diary (item 1)      |
| 3  | Basement        | Yes    | –                   |
| 4  | Child's Room    | No     | Basement Key (id 10)|
| 5  | Ritual Room     | Yes    | –                   |

---

## Chapters

| ID | Name      | Trigger                              |
|----|-----------|--------------------------------------|
| 0  | Prologue  | Game start                           |
| 1  | Chapter I | 3 or more steps taken                |
| 2  | Chapter II| Lily encountered                     |
| 3  | Chapter III| Diary found                         |
| 4  | Finale    | Ritual room reached                  |

---

## Endings

| Ending      | Condition                                          | Tone         |
|-------------|---------------------------------------------------|--------------|
| Escape      | Default path (Lily not fully trusted)             | Bittersweet  |
| Sacrifice   | Courage ≥ 70 and monster aware                   | Tragic/noble |
| Truth       | Knows truth AND Lily trusts player                | Cathartic    |
| Corruption  | Sanity < 20 OR > 5 dark choices                  | Bleak        |

---

## Items

| ID | Name          | Effect                                    |
|----|---------------|-------------------------------------------|
| 0  | Candle        | Starting item; cosmetic / atmosphere      |
| 1  | Diary         | Sets FLAG_FOUND_DIARY; needed for truth   |
| 10 | Basement Key  | Unlocks the Dark Corridor north exit      |

---

## Dialogue System

Conversations use a tree of `DialogueNode` objects. Each node has a speaker,
text, and up to `MAX_CHOICES` (8) `DialogueChoice` entries. Choices can be
gated by minimum courage and/or possession of a specific item. Selecting a
choice applies sanity/courage deltas and may set a story flag.

---

## Architecture

```
src/
  main.c       – Entry point; argument parsing; menu; calls game_init/run
  game.c       – Game loop; command dispatch; win/lose checks
  player.c     – Player struct; stat modification; inventory
  world.c      – Location graph; movement; file loader
  dialogue.c   – Dialogue tree; interactive conversation runner
  story.c      – Chapter progression; ending logic; event triggers
  ui.c         – Terminal output; HUD; menus; ANSI helpers
  utils.c      – String/file helpers; slow print; random; clamp

include/
  game.h / player.h / world.h / dialogue.h / story.h / ui.h / utils.h

assets/
  story.txt    – Narrative text
  locations.txt– Location graph (parsed by world_load_locations)

docs/
  DESIGN.md    – This document
```

---

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/paper_lily
```

Optional flags passed to the executable:

| Flag         | Meaning                              |
|--------------|--------------------------------------|
| `-n <name>`  | Set the player character name        |
| `-a <path>`  | Override the assets directory path   |
| `-s`         | Enable slow (typewriter) text output |
| `-c`         | Disable ANSI colour output           |
| `-h`         | Print usage help                     |

---

## Future Work

- Persistent save/load (binary or JSON)
- More NPCs and branching dialogue trees
- Sound cues via a simple audio library
- Procedurally generated atmospheric descriptions
- Additional endings and secret paths
