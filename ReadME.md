# subash — Project & Task Organizer

`subash` is a fast, keyboard-driven TUI (written in C + notcurses) that lets
you organize projects and their tasks, jump into their working directories,
open Obsidian docs, follow Redmine / Gerrit links, and peek at a compact git
log for each project — all from the terminal.

The command is a shell function (see `subash.sh`) that runs the compiled
binary and then evaluates a `cd` command written to `/tmp/subash_action` so
the parent shell actually changes directory.

---

## Concepts

- **Project** — a workspace (usually a git repo). Details:
  - `description` — short blurb, shown in the header card
  - `git_path`    — where `git log --graph` runs
  - `gerrit_url`  — the Gerrit URL for the project
  - `redmine_url` — the Redmine project URL

- **Task** — belongs to a project. Fields:
  - `name`, `path` (local working dir)
  - `obsidian`      — path to a `.md` note (opened with `xdg-open`)
  - `remote_host`, `remote_user`, `remote_pass` — an SSH endpoint
  - `redmine`       — a Redmine issue URL
  - `gerrit_commit` — a Gerrit commit hash or URL
  - `notes`

> The internal config key `product = ...` is kept for backward compatibility;
> it means the same as "task".

---

## Keybindings

### Global
| Key        | Action                              |
|------------|-------------------------------------|
| `?` / `F1` | Toggle this manual inside the TUI    |
| `q` / `Esc`| Quit                                 |

### Navigation
| Key         | Action                              |
|-------------|-------------------------------------|
| `←/→` `h/l` | Switch pane focus (Projects ↔ Tasks) |
| `↑/↓` `j/k` | Move selection in the focused pane   |
| `Enter`     | `cd` into the selected task's `local_path` (exits the TUI) |

### Manage
| Key | Action                          |
|-----|---------------------------------|
| `a` | Add task (full-screen form)     |
| `n` | New project (full-screen form)  |
| `e` | Edit selected task              |
| `P` | Edit selected project details   |
| `d` | Delete selected task            |
| `D` | Delete selected project (cascades) |

### Open external
| Key | Action                                    |
|-----|-------------------------------------------|
| `o` | Open the selected task's Obsidian doc     |
| `g` | Open the selected task's Gerrit commit URL (falls back to the project's Gerrit URL) |
| `r` | Open Redmine (task-level first, else project-level) |
| `c` | Copy the selected task's `local_path` to the clipboard (wl-copy → xclip) |

You can also **click** an underlined `.md` path in the task table to open it.

### Search
| Key   | Action                                       |
|-------|----------------------------------------------|
| `/`   | Open fuzzy filter for tasks in the current project |
| `Esc` | Clear the filter                              |
| `Enter` | Pick the first match                         |

---

## CLI mode

    subash <name>

Jumps directly to a project or task whose name matches. If a task matches,
`cd`s into its `local_path`; if a project matches, `cd`s into its first
task's `local_path`.

---

## Configuration file — `~/.subash.conf`

Plain text. Reloaded on launch. Feel free to hand-edit — the schema is
line-oriented and forgiving.

```conf
selected_project = 0
selected_product = 0

project = MyProject
  description = short blurb shown in the header card
  git_path    = ~/code/myproject
  gerrit_url  = https://gerrit.example.com/#/q/project:MyProject
  redmine_url = https://redmine.example.com/projects/myproject

  product = task-A
    path          = ~/code/myproject/taskA
    obsidian      = ~/Obsidian/Notes/taskA.md
    remote_host   = user@10.0.0.5:/opt/app
    remote_user   = user
    remote_pass   = secret
    redmine       = https://redmine.example.com/issues/1234
    gerrit_commit = abc123def
    notes         = free-form text

  product = task-B
    path = ~/code/myproject/taskB
```

`~/` and `$HOME/` are expanded whenever a path is used (jumping, opening
files, checking existence), but the stored value keeps the `~` prefix so
the file remains portable.

---

## Status dots in the task table

| Dot        | Meaning                                              |
|------------|------------------------------------------------------|
| `●` green  | `local_path` exists on disk                          |
| `○` yellow | Remote-only — no `local_path`, but `remote_host` set |
| `○` grey   | Path missing                                         |

---

## Git log pane

When a project has a `git_path` set, `subash` runs

    git -C <git_path> log --graph --oneline --decorate --all -n 40

once per project switch and renders the output in a compact pane below
the tasks table. Branch characters (`* | / \`) are cyan; commit hashes
are orange.

---

## Build & install

    make               # produces ./subash-bin
    make install       # copies to ~/.local/bin/subash-bin
    source subash.sh   # exposes the `subash` shell function

Then either add `source /path/to/subash.sh` to your `~/.bashrc`, or add
`~/.local/bin` to `PATH` and invoke the binary directly (for CLI mode).

---

## Reporting issues

The project lives at
<https://github.com/rajeevkr945/AI-Assistant/tree/TUI_port_to_C>.
