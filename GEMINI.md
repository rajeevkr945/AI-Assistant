# Project Context: dev-flow

You are an expert AI software engineer helping to build `dev-flow`, a high-performance terminal workspace organizer and context switcher written in Rust and wrapped in Bash. 

## Core Principles
- **Minimalist & Blazing Fast:** Keep binary startup time minimal ($\le10\text{ms}$). Avoid unnecessary runtimes or heavy macro overhead.
- **Strict Architecture Split:** - **Rust** owns the core state machine, data models, serializing/deserializing data to `~/.dev-flow.json`, and rendering the TUI.
  - **Bash** handles environmental mutations that a compiled binary cannot execute directly (e.g., executing `cd`, sourcing project env files).
- **Separation of Concerns:** Keep `models.rs` (data structures), `app.rs` (state engine), and `ui.rs` (Ratatui rendering code) completely decoupled.

## Tech Stack
- **Language:** Rust (Stable)
- **TUI Framework:** `ratatui` (v0.26) with `crossterm` (v0.27) backend
- **Serialization:** `serde` and `serde_json` for flat-file local persistence
- **Shell Layer:** Bash functions via temporary IPC handling file triggers (`/tmp/dev_flow_action`)

## Current Task Status
- [x] Linux environment setup & rustup verification
- [x] Dependencies configured in `Cargo.toml`
- [x] Implement `src/models.rs` data structures
- [x] Implement `src/app.rs` keyboard loop & state machine
- [ ] Implement `src/ui.rs` dual-column rendering layout
- [ ] Create the Bash wrapper functions for environment manipulation

When asked to generate or modify code, ensure it aligns strictly with this split architecture blueprint.