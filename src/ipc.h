#ifndef IPC_H
#define IPC_H

// Writes a `cd '<path>' && ls` command to /tmp/subash_action.
// Returns 0 on success, or -1 on error.
int trigger_bash_action(const char *path);

// Writes a `xdg-open '<target>' &>/dev/null &` command to /tmp/subash_action.
// Returns 0 on success, or -1 on error.
int trigger_open_action(const char *target);

// Writes a clipboard-copy command to /tmp/subash_action. Prefers wl-copy
// (Wayland) then xclip (X11). Returns 0 on success, or -1 on error.
int trigger_copy_action(const char *text);

#endif // IPC_H
