# Tetris Battle Report

## Project Summary

Tetris Battle is an embedded implementation of two-player Tetris that targets two ESP32-S3 development boards. Each player has a local screen, physical controls, saved user settings, music output, and a wireless ESP-NOW link to the other player. This project is an arcade-style embedded game built to make competitive Tetris playable without a laptop, phone, or wired connection between players.

The game supports sign-in, per-user settings, a shared lobby, configurable match options, two game modes (Marathon and Sprint 40), pause/restart/quit confirmation flows, Tetris music, and head-to-head garbage attacks.

## Physical Components

- Two [ESP32-S3 development boards](https://www.amazon.com/dp/B0F5QCK6X5?psc=1&ref=ppx_pop_dt_b_asin_title). Each board runs one player station and is powered over USB-C from a phone or laptop.
- Two [480x320 SPI TFT LCD displays](https://www.amazon.com/dp/B0CKRJ81B5?psc=1&ref=ppx_pop_dt_b_asin_title).
- Eighteen [momentary pushbuttons](https://www.amazon.com/dp/B0D8T65LRG?psc=1&ref=ppx_pop_dt_b_asin_title), nine per player. The buttons form left and right directional clusters plus a center button used for selection, pause, and a three-second software reset.
- Two passive buzzers. The buzzer on each unit plays the Tetris theme through PWM/LEDC when music is enabled.
- Hookup wire, jumper wires, and headers. These connect the ESP32-S3, display, buttons, and buzzer.
- Protoboard. This makes the wiring mechanically stable compared with a loose breadboard.
- 3D-printed chassis parts. The resin front cover exposes the LCD and provides button mounting holes. The PLA container holds the microcontroller, protoboard, and wiring. Both parts are combined in `front-cover-and-container.3mf` for printing.

## Game Logic

The game implements:

- A 10x20 visible board.
- The seven standard tetrominoes.
- A seven-bag randomizer.
- Level-based gravity.
- Score values of 40/100/300/1200 times `level + 1`.
- Hold support, ghost pieces, and up to five next-piece previews.

Standard tetromino shapes, SRS rotation states, and wall kick data were based primarily on the Tetris Wiki. Because the project uses screen coordinates where positive `y` points downward, the SRS kick table `y` values are inverted in the implementation.

The "battle" behavior comes from garbage attacks and synchronized match flow. When garbage is enabled, a double sends one garbage line, a triple sends two, and a Tetris sends four. The receiving board queues those garbage lines and inserts them from the bottom with one open hole column after the active piece locks. Both players start from a shared seed, so the piece stream is fair at the start of a match, while player decisions and received garbage make the boards diverge. The game also exchanges start, pause, restart, quit, garbage, and game-over packets so both devices agree on match state.

## Software Architecture

The program is an Arduino/PlatformIO project using FreeRTOS tasks. Core 0 is reserved for wireless work; Core 1 runs the application tasks.

- `WirelessTask` owns Wi-Fi/ESP-NOW. It sends periodic presence packets, receives peer packets through the ESP-NOW callback, reports transport events to the game task, and retransmits reliable packets until an ACK is received.
- `InputTask` polls all buttons every 5 ms, debounces them, emits press/release/repeat events, and performs the three-second center-button reset.
- `GameTask` is the coordinator. It consumes input events, remote events, and storage responses; advances the game engine every 5 ms; emits outbound network packets; emits music commands; and overwrites the render queue with the newest screen state.
- `RenderTask` owns the TFT display. It receives display config or screen render events and draws only the current visual state.
- `StorageTask` owns NVS through `Preferences`. It loads and saves user settings keyed by username.
- `MusicTask` owns the buzzer. It starts, pauses, resumes, or stops the Korobeiniki track based on music events from the game task.

There is no shared state between tasks; rather, information moves through queues:

- `InputTask -> GameTask`: `g_inputEventQueue` carries button press, release, and repeat events.
- `WirelessTask -> GameTask`: `g_remoteEventQueue` carries transport-ready, peer-disconnect, and received-packet events.
- `GameTask -> WirelessTask`: `g_outboundPacketQueue` carries packets that must be sent to the other player.
- `GameTask -> RenderTask`: `g_renderEventQueue` carries display config and screen state. It is intentionally length one, so rendering follows the latest state instead of building a stale backlog.
- `GameTask <-> StorageTask`: `g_storageRequestQueue` and `g_storageResponseQueue` carry NVS load/save requests and responses during sign-in and settings saves.
- `GameTask -> MusicTask`: `g_musicEventQueue` carries start, stop, pause, and resume commands when matches start, pause, resume, end, or disable music.

## Problems Encountered and Future Improvements

The largest implementation problem I faced was display refresh rate. Rewriting whole screens was visibly slow on the SPI TFT, especially during gameplay where every movement could force a large redraw. I fixed this by making rendering more granular. Gameplay now compares previous and current board cells, then redraws only changed cells. Separate small refresh windows handle hold, next preview, score, lines, level, garbage warning, username slots, lobby rows, and menu selections. Full redraws are now reserved for screen changes, theme changes, and a few cases where the whole layout must change.

There is still a known pause-state bug. If one player loses and the still-active player later enters the pause menu, the game-over player does not enter that same pause menu. As a result, the game-over player cannot recognize or confirm any pause-menu action from the still-active player. The still-active player becomes stuck waiting for a confirmation that will never arrive. The workaround is for both players to hold the center button for three seconds, reset both devices, sign in again, and re-enter the lobby. The cleanest fix is to let the game-over player receive the pause menu, even though that player cannot trigger it.

NVS currently saves user settings, including username, theme, hold, ghost, and next-preview preferences, but it does not save high scores. For an arcade-style game, persistent high scores are expected. A future version should store per-user and possibly per-mode high scores, then show them in the menu or game-over screen.

## Similarity to Real Embedded Systems

This project is similar to real embedded systems because it is event-driven, task-based, hardware-constrained, and stateful across power cycles. Like many embedded products, it separates hardware ownership by task: one task owns wireless transport, one owns display drawing, one owns input polling, one owns persistent storage, and one owns audio. The queue-based design also resembles production firmware, where subsystems communicate through bounded messages rather than shared blocking calls.

Projects such as `zamweis/t-display-s3-tetris` and `williangaspar/arduino-tetris` also recreate Tetris on small embedded hardware, but they are essentially single-player local games. `ianseddon/arduino-tetris-battle` is the closer comparison because it implements multiplayer battle Tetris, but it uses a wired serial connection and is not structured around an RTOS. This project uses two independent ESP32-S3 systems connected by ESP-NOW, with presence, reliable packet retry, ACKs, lobby settings synchronization, pause confirmation, garbage attacks, and game-over result exchange. That makes the firmware closer to a distributed embedded application than a standalone toy.

Compared with real commercial embedded systems, the project is simpler: it has only two fixed peers, no secure provisioning, no battery/power-management layer, no formal diagnostics, and no field-update path. However, the physical design, soldered wiring, dedicated controls, enclosure, display, NVS settings, wireless protocol, and FreeRTOS architecture make it substantially more complete than a temporary breadboard demo.

## Documentation Files

- `circuit_diagram.drawio.xml`: editable diagrams.net circuit diagram
- `circuit_diagram.pdf`: exported PDF version of the circuit diagram.
- `front-cover-and-container.3mf`: combined 3D-print file for the resin front cover and PLA container.
