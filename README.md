# A.I.R.A. — Technical Review Repository

A.I.R.A. is a science-fiction drone game developed in Unreal Engine 5 by **ArtSea Games LLC**.

This repository is a focused technical-review package prepared for the **President Tech Award**. It contains project C++ source used by the current playable vertical slice, documentation of the C++/Blueprint architecture, and selected visual evidence from the prototype and development workflow.

## Current playable systems demonstrated

- Main menu and pause menu flow
- Save/load and player-state persistence
- Game-over flow and return to the main menu
- Mission briefing and mission HUD
- Runtime discovery of relay and Primary Transmission actors
- Integration with Blueprint relay proximity and unlock state
- Relay-state tracking and `8 / 8` progression
- Primary Transmission gating and interaction
- Multi-stage diagnostic transmission
- Mission completion and return-to-menu flow
- Gameplay/menu audio and display settings

## Repository structure

```text
Source/
  Drone.cpp
  DroneMenuSystem.cpp
  DroneMenuSystem.h

Documentation/
  CODE_OVERVIEW.md
  VISUAL_OVERVIEW.md

Screenshots/
  01_Relay_Station_Environment.jpg
  02_Relay_Network_Overview.jpg
  03_Hostile_Drone_In_Environment.jpg
  04_AIRA_and_Hostile_Drone.jpg
  05_Blueprint_DronePawn_EventGraph.png
  06_Blueprint_EnergyPickup_EventGraph.png
  07_Enemy_Drone_Blender_Wireframe.png
  08_Orbital_Station_Scene.jpg
```

## Playable Demo

A playable vertical slice of A.I.R.A. demonstrates the relay-network mission,
hostile drones, mission HUD, Primary Transmission System and mission completion.

https://www.youtube.com/watch?v=l9h4iGE6bBo 

## Unreal Engine architecture

A.I.R.A. uses a hybrid Unreal Engine architecture:

- **C++** handles persistent game flow, menus, save/load, game-over, mission orchestration, relay progression, Primary Transmission logic, mission completion, and related UI/audio flow.
- **Blueprints** handle additional gameplay systems such as the drone pawn/control, movement and firing input, enemies, projectiles, damage, energy pickups/healing, spawning, and parts of the HUD.

Binary Unreal assets (`.uasset`, `.umap`) and licensed third-party content are intentionally excluded from this public repository.

## Visual evidence

### In-engine relay-station environment

![Relay station environment](Screenshots/01_Relay_Station_Environment.jpg)

### A.I.R.A. and hostile drone

![A.I.R.A. and hostile drone](Screenshots/04_AIRA_and_Hostile_Drone.jpg)

### Blueprint gameplay implementation

![Drone Pawn Blueprint Event Graph](Screenshots/05_Blueprint_DronePawn_EventGraph.png)

### 3D asset workflow

![Enemy drone Blender wireframe](Screenshots/07_Enemy_Drone_Blender_Wireframe.png)

See [`Documentation/VISUAL_OVERVIEW.md`](Documentation/VISUAL_OVERVIEW.md) for the complete visual set and short technical captions.

## Technical documentation

See [`Documentation/CODE_OVERVIEW.md`](Documentation/CODE_OVERVIEW.md) for a file-by-file explanation of the exported source and its role in the playable vertical slice.

## Flight Boost / Afterburner System

A.I.R.A. includes a native C++ afterburner system for the player-controlled drone.

Hold **LEFT SHIFT** while flying to engage the boost. The system increases the drone's maximum flight speed and acceleration by **3x**, using the existing `UFloatingPawnMovement` component without replacing the normal movement system.

The afterburner uses an independent rechargeable **BoostEnergy** resource:

- 100 maximum BoostEnergy
- approximately 10 seconds of continuous moving boost
- 1.5-second recharge delay
- approximately 15 seconds for a full recharge
- partial charge can be reused at any time
- automatic boost shutdown when the battery reaches 0%

A native Unreal Slate HUD displays the BOOST battery, percentage, and current state on the right side of the gameplay viewport.

The feature is implemented in C++ and has been integrated and tested with the player drone in the current A.I.R.A. playable prototype.

## Rights and publication scope

Copyright © 2026 ArtSea Games LLC. All rights reserved.

The source and project images in this repository are published for technical review and evaluation. No open-source license or permission to redistribute project assets is granted. Third-party source code and licensed binary content are not included.
