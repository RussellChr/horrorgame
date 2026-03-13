# Metamorph

## Project Description
This project is a horror-themed game that immerses players in a chilling narrative, exploring the boundaries of fear and reality.

## File Documentation
The following files contribute to the project:
- **main.py**: The entry point for the game, handling initialization and game loop.
- **settings.py**: Configuration file where game settings can be adjusted.
- **assets/**: Directory containing game assets such as images and sounds.
- **src/**: Source code folder containing all game logic and structure.
- **README.md**: Documentation file (this file) that gives an overview of the project and instructions for use.

Metamorph/
├── README.md              Documentation (updated)
├── CMakeLists.txt         Build configuration
├── .gitignore             Git ignore rules
├── src/                   C source files
│   ├── main.c             Entry point
│   ├── game.c             Main game loop and dispatcher
│   ├── player.c           Player stats, inventory, flags
│   ├── world.c            Location graph and movement
│   ├── dialogue.c         Dialogue trees and conversations
│   ├── story.c            Chapter progression and endings
│   ├── ui.c               Terminal/UI rendering
│   ├── render.c           Graphics rendering
│   ├── animation.c        Animation system
│   ├── camera.c           Camera control
│   ├── collision.c        Collision detection
│   └── utils.c            Utility functions
├── include/               Header files
│   ├── player.h           Player data structures
│   ├── world.h            World/location data
│   ├── dialogue.h         Dialogue tree system
│   ├── story.h            Story progression
│   ├── ui.h               UI components
│   ├── render.h           Rendering functions
│   ├── animation.h        Animation definitions
│   ├── camera.h           Camera system
│   ├── collision.h        Collision structures
│   └── utils.h            Utility functions
├── assets/                Game assets
│   ├── story.txt          Narrative content
│   ├── locations.txt      Location definitions
│   └── dialogue.png       Dialogue font/UI graphics
├── maps/                  Game level/room data
│   └── rooms.txt          Room layout definitions
└── scripts/               Game scripts
    └── dialogue.txt       Dialogue scripts
