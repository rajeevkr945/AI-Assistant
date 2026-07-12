# Project Context: subash

You are an expert AI software engineer helping to build `subash`, a high-performance terminal workspace organizer and context switcher written in C and wrapped in Bash. 

## Core Principles
- **Minimalist & Blazing Fast:** Keep binary startup time minimal ($\le5\text{ms}$).
- **Strict Architecture Split:**
  - **C** owns the core state machine, data models, loading/saving configurations to `~/.subash.conf`, and rendering the TUI (using `notcurses`).
  - **Bash** handles environmental mutations (e.g., executing `cd`, sourcing project env files) triggered via IPC file (`/tmp/subash_action`).
- **Separation of Concerns:** Keep models, state engine, and rendering code completely decoupled.

## Tech Stack
- **Language:** C (C11 standard)
- **TUI Library:** `notcurses`
- **Shell Layer:** Bash functions via temporary IPC handling file triggers (`/tmp/subash_action`)

## Current Task Status
- [x] Milestone 1: The Core Data Layer (`src/models.h` / `src/models.c` configuration loading/saving `~/.subash.conf`)
- [x] Milestone 2: Headless Parameter Jumper and Shell Wrapper (`src/ipc.h` / `src/ipc.c` / `src/main.c` / `subash.sh`)
- [x] Milestone 3: UI Rendering (using `notcurses` / `src/ui.c`)
- [x] Milestone 4: TUI-to-Shell Interlocking & Final Polishing