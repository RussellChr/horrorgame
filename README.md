

## Project Structure

```
Game/
├── src/               C source files
│   ├── main.c         Entry point
│   ├── game.c         Main game loop and command dispatcher
│   ├── player.c       Player stats, inventory, flags
│   ├── world.c        Location graph and movement
│   ├── dialogue.c     Dialogue tree and interactive conversations
│   ├── story.c        Chapter progression and endings
│   ├── ui.c           Terminal rendering and menus
│   └── utils.c        String, file, and random helpers
├── include/           Header files
├── assets/
│   ├── story.txt      Narrative content
│   └── locations.txt  Location definitions
├── docs/
│   └── DESIGN.md      Full game design document
└── CMakeLists.txt     Cross-platform build configuration
```

