#ifndef IPC_H
#define IPC_H

// Writes a cd command to /tmp/subash_action.
// Returns 0 on success, or -1 on error.
int trigger_bash_action(const char *path);

#endif // IPC_H
