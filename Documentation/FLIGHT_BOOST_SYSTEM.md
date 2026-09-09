# A.I.R.A. Flight Boost System

## Overview

A native Unreal Engine 5 afterburner system for the player-controlled A.I.R.A. drone. It extends the existing pawn through an optional C++ actor component and supplies its own native Slate battery indicator.

## Gameplay

Hold LEFT SHIFT to activate the afterburner. Existing movement input determines direction: W flies forward normally; W + LEFT SHIFT flies forward with boosted maximum speed and acceleration.

The independent **BoostEnergy** resource starts full. Default settings provide 3x maximum flight speed, 3x acceleration, and approximately 10 seconds of continuous moving boost. Charge drains only while the boost is active and the movement component has pending input or velocity above 1 Unreal unit per second. Coasting counts as movement; a stationary LEFT SHIFT hold does not consume charge or recharge the battery.

Release LEFT SHIFT to stop boost and restore the captured movement settings immediately. Existing momentum decelerates through UFloatingPawnMovement using the original deceleration; the component never teleports the actor, rewrites movement input, or sets velocity.

At zero charge, boost shuts down automatically and normal flight continues. The player must release LEFT SHIFT after depletion before another hold can activate boost. This prevents rapid activation/depletion cycling while LEFT SHIFT remains held. Recharge continues while LEFT SHIFT remains held after depletion. Once LEFT SHIFT has been released, any positive charge is reusable; a 35-point charge provides approximately 3.5 seconds of moving boost.

## Balance

All six editable settings use the category **A.I.R.A.Flight Boost**.

| Setting | Default |
| --- | --- |
| MaxBoostEnergy | 100 |
| BoostSpeedMultiplier | 3 |
| BoostAccelerationMultiplier | 3 |
| ContinuousBoostDuration | 10 seconds |
| RechargeDelay | 1.5 seconds |
| FullRechargeDuration | 15 seconds |

Drain rate is derived as MaxBoostEnergy / ContinuousBoostDuration: **10 points/second**.
Recharge rate is derived as MaxBoostEnergy / FullRechargeDuration: **6.6667 points/second**.

Recharge begins after the delay following deactivation and is gradual. Empty-to-full recharge takes approximately **16.5 seconds including the delay**, or 15 seconds after the delay. Battery values are clamped between zero and capacity; the percentage getter returns 0–1. Timing uses game time and pauses with gameplay. Rates are derived getters rather than independently editable properties so duration settings remain authoritative.

## Architecture

- **UAIRAFlightBoostComponent** is a Blueprint-spawnable UActorComponent. Add one instance to BP_BotPawn.
- **UFloatingPawnMovement** was confirmed through the Unreal Editor object API on BP_BotPawn's Floating Pawn Movement component. The existing Handle Floating Pawn Movement graph calls AddInputVector for right, forward, and up movement.
- BeginPlay logs the captured speed and acceleration once and captures OriginalMaxSpeed, OriginalAcceleration, OriginalDeceleration, and OriginalTurningBoost. The inspected live Level_1 pawn values were 1200, 4000, 8000, and 8 respectively. These numbers are not hard-coded into the boost implementation.
- Boost scales only maximum speed and acceleration. Deactivation and EndPlay restore all four captured values. The movement component ticks after the boost component.
- LEFT SHIFT is polled using APlayerController::IsInputKeyDown(EKeys::LeftShift). No Enhanced Input assets, mappings, or input bindings are changed.
- Only the locally controlled player pawn can engage boost. Existing controller movement-input locks and world pause prevent activation.
- SAIRAFlightBoostHUD uses a weak component reference. The component owns one main-viewport overlay widget for the local player, removes it on loss of local ownership or EndPlay/destruction, and guards against duplicate component instances.
- BlueprintPure getters expose active state, energy, normalized percentage, and derived rates.
- LogAIRAFlightBoost logs initialization and state transitions, including activation with original-to-boosted values, deactivation with restored values, depletion, recharge start, full recharge, HUD mounting, and HUD removal. It does not emit per-frame status logs.

The module already provides Engine, InputCore, Slate, and SlateCore dependencies. No Build.cs modification was necessary. The exported sources retain DRONE_API and belong in Source/Drone in this project.

## HUD

A compact native Slate panel appears at the right-center of the player's viewport, with a 50 Slate-unit right margin and a 136-unit panel width. It shows **BOOST**, a continuous progress bar, percentage, and a small status line.

The panel uses a dark translucent background and cyan charge color. Charge below 25% is amber; charge below 10% is red. The bar reflects 100%, 50%, 10%, and 0% directly. Displayed percentages round upward so a usable fractional charge is not labeled 0%. Status labels show BOOST READY, BOOST ACTIVE, RECHARGING, and BOOST DEPLETED.

It is non-interactive and does not consume pointer or keyboard input. It uses AddViewportWidgetContent at Z-order 1000, above normal mission HUD layers (500-510) and below modal overlays and menus (9000-10000). The existing circular HUD is not replaced or modified. No Widget Blueprint or replacement game HUD class is required. Exact layout against the running Level_1 HUD still requires the manual visual check below.

## Safety

Normal movement values are captured at runtime and restored after boost. **BoostEnergy is entirely separate from the existing player Energy property.** The component performs no reflection lookup or modification of player Energy.

Unrelated existing source, Blueprint assets, Enhanced Input configuration, relay missions, Primary Transmission, enemies, projectiles, health/energy gameplay, save/load, Main Menu, Pause Menu, and Game Over were not modified. The component has no save-game persistence or multiplayer replication implementation; boost starts full on each pawn BeginPlay.

Only attach the component to the player drone. If the owner has no UFloatingPawnMovement, it disables itself and logs an error. Another future system that writes the same movement settings during boost could conflict with restoration of the BeginPlay snapshot.

## Build and validation

The Drone Win64 Development game target compiled and linked successfully using Unreal Engine 5.8 and the Visual Studio 14.44 toolchain.

The revised DroneEditor target was not built because Unreal Editor was still open, as requested. The editor was not terminated. Close Unreal Editor and Live Coding normally, then run:

```powershell
& 'E:\UE5\UE_5.8\Engine\Build\BatchFiles\Build.bat' DroneEditor Win64 Development '-Project=E:\UE_Projects\DroneGame\Drone.uproject' -WaitMutex
```

Reopen the project after that build to load the new reflected component class. Do not rely on the already-running editor to discover the new class.

Read-only inspection confirmed the pawn movement class and its AddInputVector integration. Source review checked drain/recharge logic, depletion release gating, restoration, ownership, and HUD cleanup. For this revision, Source and Config files were hash-checked against a pre-change baseline; only the two Flight Boost implementation files changed. No asset editing tools were used. Exported sources were hash-checked against the compiled originals.

Read-only inspection of the current Level_1 PIE player pawn confirmed runtime MaxSpeed=1200 and Acceleration=4000. The default boost therefore sets MaxSpeed=3600 and Acceleration=12000. These boosted values have not yet been observed in play. The inspected live pawn and loaded BP_BotPawn component lists contained no AIRAFlightBoostComponent. This explains why that session could not display its HUD or engage boost. BP_BotPawn remains untouched; attach the component after rebuilding and reopening the editor. Revised gameplay timing, input, and HUD appearance still require the manual test below.

## Unreal Editor integration and manual test checklist

Prerequisite: complete the DroneEditor build with the editor and Live Coding closed, then reopen the project.

1. Open Content/Blueprints/BotPawn/BP_BotPawn.
2. Choose Add Component, search for AIRAFlightBoostComponent, and add exactly one instance.
3. Compile the Blueprint.
4. Save the Blueprint.
5. Start Level_1 gameplay. Allow the existing intro/input-enable sequence to finish and focus the game viewport.
6. Fly with W.
7. Compare normal speed against the original game behavior.
8. Hold W + LEFT SHIFT.
9. Confirm approximately 3x faster maximum flight speed after acceleration.
10. Confirm the BOOST battery drains over approximately 10 seconds of continuous moving boost.
11. Release LEFT SHIFT.
12. Confirm normal maximum speed and acceleration are restored immediately and excess momentum decelerates normally.
13. Confirm recharge starts after approximately 1.5 seconds.
14. Confirm gradual refill, taking approximately 15 seconds from empty after the delay.
15. Release and use LEFT SHIFT again while partially charged.
16. Confirm boost works using the remaining energy.
17. Drain the battery completely.
18. Confirm boost disables at 0%. Keep LEFT SHIFT held and confirm no repeated boost activation while charge regenerates; release and hold LEFT SHIFT again to reuse charge.
19. Confirm normal flight still works at 0%.
20. Confirm relay missions, Primary Transmission, firing, camera, pause menu, and the existing HUD remain functional.

Additional checks: observe the bar at 100%, 50%, 10%, and 0%; hold LEFT SHIFT while stationary; pause during boost; resume and release LEFT SHIFT; leave gameplay and reload Level_1 several times to check cleanup and absence of duplicate indicators.

## Export

This folder contains copies of the successfully compiled source files, not moved originals. Nothing was uploaded to GitHub. PTA_GitHub_Ready was not modified.

