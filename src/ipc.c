#include "ipc.h"
#include <stdio.h>
#include <string.h>

// Escape single quotes for safe embedding inside a bash single-quoted string.
// Writes 'text-with-single-quotes-escaped' style output to f.
static void write_squoted(FILE *f, const char *s) {
    fputc('\'', f);
    for (const char *p = s; *p; p++) {
        if (*p == '\'') fputs("'\\''", f);
        else fputc(*p, f);
    }
    fputc('\'', f);
}

int trigger_bash_action(const char *path) {
    FILE *f = fopen("/tmp/subash_action", "w");
    if (!f) return -1;
    fputs("cd ", f);
    write_squoted(f, path);
    fputs(" && ls\n", f);
    fclose(f);
    return 0;
}

int trigger_open_action(const char *target) {
    if (!target || !*target) return -1;
    FILE *f = fopen("/tmp/subash_action", "w");
    if (!f) return -1;
    fputs("xdg-open ", f);
    write_squoted(f, target);
    fputs(" >/dev/null 2>&1 &\n", f);
    fclose(f);
    return 0;
}

int trigger_copy_action(const char *text) {
    if (!text) return -1;
    FILE *f = fopen("/tmp/subash_action", "w");
    if (!f) return -1;
    // Prefer wl-copy on Wayland, fall back to xclip.
    fputs("if command -v wl-copy >/dev/null 2>&1; then printf %s ", f);
    write_squoted(f, text);
    fputs(" | wl-copy; ", f);
    fputs("elif command -v xclip >/dev/null 2>&1; then printf %s ", f);
    write_squoted(f, text);
    fputs(" | xclip -selection clipboard; fi\n", f);
    fclose(f);
    return 0;
}
