#ifndef UI_H
#define UI_H

#include "models.h"
#include <notcurses/notcurses.h>

// Draw the dual-column split screen TUI interface.
void draw_interface(struct notcurses* nc, struct ncplane* left, struct ncplane* right, AppState* state);

#endif // UI_H
