# A.I.R.A. Technical Source Overview

## Export scope

This repository contains the project-authored C++ source files used by the current playable A.I.R.A. vertical slice.

The Unreal Engine project also uses Blueprint-based gameplay systems. Binary `.uasset` / `.umap` files, generated output, Unreal Engine source, plugin source, and licensed third-party content are intentionally excluded from this public technical repository.

## `DroneMenuSystem.cpp`

Original project path: `Source/Drone/DroneMenuSystem.cpp`

This is the project's main native gameplay-flow implementation. It builds the Slate-based main menu, pause menu, settings, save/load interface, mission briefing, mission HUD, game-over display, and mission-complete display. It also coordinates world transitions, player-death observation, save-game restoration, gameplay audio, and the relay-network mission used by the current playable vertical slice.

### Demonstrated functionality

- Main-menu and pause-menu presentation and input handling
- Play, save, load, settings, exit, and resume flows
- Save metadata, player transform, current level, and player energy persistence
- Game-over detection through pawn destruction and the Blueprint-exposed `Energy` property
- Game-over timing, presentation, and return to the startup main menu
- Mission briefing and objective HUD rendered with Unreal Slate
- Runtime discovery of relay and Primary Transmission actors
- Integration with Blueprint relay proximity and unlock state
- Relay-state tracking and unlocked-relay counting
- Primary Transmission proximity validation and interaction input
- Gating of the Primary Transmission System until all discovered relay nodes are online
- Multi-stage diagnostic transmission sequence and messaging
- Mission-complete presentation and return to the startup main menu
- Menu/gameplay music, interaction sounds, and volume/display settings
- Runtime menu-scene presentation for the A.I.R.A. drone

### Why this is useful technical evidence

The file shows an integrated UE5 implementation rather than a mock-up: it connects engine lifecycle callbacks, maps, actors, player input, reflected Blueprint data, save games, audio, viewport UI, mission state, and level travel. The relay-to-primary-transmission sequence directly corresponds to the progression and completion conditions demonstrated in the playable build.

For ordinary relay nodes, C++ reads the Blueprint-exposed `bPlayerInRange` and `bUnlocked` state and tracks mission progression. For the Primary Transmission System, C++ validates proximity, checks whether the full relay network is online, processes the interaction input, advances the transmission stages, and triggers mission completion.

## `DroneMenuSystem.h`

Original project path: `Source/Drone/DroneMenuSystem.h`

This header defines the native types and persistent state used by the implementation: save metadata, save-game objects, the menu game mode, mission-device state, and the custom game instance that owns menu, game-over, mission, audio, settings, save/load, and transition flow.

### Demonstrated functionality

- Unreal-reflected save-game data types
- Save index and per-save state, including level, transform, timestamp, and energy
- Blueprint-callable game-over entry point
- Relay-device state and weak actor references
- Native mission state for relay count, Primary Transmission, staged completion, and UI timing
- Lifecycle and ticker ownership for setup and cleanup
- Separation between persistent game-instance flow and level-owned actors

### Why this is useful technical evidence

The declarations expose the architecture behind the running systems and show how the C++ integrates with UE5 reflection, Blueprints, world lifecycle, Slate UI, save games, audio components, timers, and actor references.

## `Drone.cpp`

Original project path: `Source/Drone/Drone.cpp`

This file registers `Drone` as the Unreal Engine project's primary game module.

### Demonstrated functionality

- Native UE5 module entry point
- Linkage between the Unreal project and its project-authored C++ module

### Why this is useful technical evidence

Although intentionally small, this is the module bootstrap required for Unreal Engine to load the project's native code. Including it keeps the public source export structurally complete.

## Gameplay implemented primarily in Blueprints

The project uses Blueprint assets for additional gameplay systems, including the configured game mode, drone pawn/control, movement and firing input, enemy bots and boss behavior, projectile behavior, damage interface, energy pickup/healing, spawning, and parts of the HUD.

The C++ systems interoperate with Blueprint gameplay through Unreal reflection and actor state. For example, the save/game-over flow reads and writes the Blueprint player's reflected `Energy` property, while the mission flow reads Blueprint relay state and controls higher-level relay progression and Primary Transmission sequencing.

Binary Blueprint assets are deliberately not included in this public source export.

## Public-export review

The exported source should contain no passwords, API keys, access tokens, private keys, private URLs, personal contact information, generated binaries, Unreal Engine source, plugin source, or Marketplace/Fab asset content.

Before publishing future updates, review new files for secrets and third-party licensing restrictions. Do not add the project's binary `Content` directory unless the rights for every included asset have been separately verified.
