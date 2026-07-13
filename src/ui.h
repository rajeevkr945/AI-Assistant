#ifndef UI_H
#define UI_H

#include "models.h"
#include <stdint.h>
#include <notcurses/notcurses.h>

// ── View modes ────────────────────────────────────────────────────────────────
// Shared by main.c and ui.c so the renderer can branch on the current mode.
typedef enum {
    MODE_INTRO,   // startup scale-in animation (5 frames, 150 ms total)
    MODE_LIST,    // normal dual-pane workspace list
    MODE_FORM     // modal product-creation form
} ViewMode;

// ── Form sub-state ────────────────────────────────────────────────────────────
typedef enum {
    FORM_FIELD_NAME = 0,
    FORM_FIELD_PATH = 1
} FormField;

typedef struct {
    char new_name[MAX_NAME];   // Product Name input buffer
    char new_path[MAX_PATH];   // Local Path input buffer
    FormField active_field;    // Which field has keyboard focus
    char error_msg[128];       // Non-empty = validation error displayed at bottom
} FormState;

// ── Animation state ───────────────────────────────────────────────────────────
// Owned by main, passed by pointer into draw_* functions so no global state.
typedef struct {
    uint64_t frame_tick;       // Incremented every 50 ms; drives the matrix widget
    int      intro_frame;      // 0-4 during MODE_INTRO scale-in; 5 = done
} AnimState;

// Number of intro animation frames and total duration
#define INTRO_FRAMES        5
#define INTRO_FRAME_MS      30    /* 5 × 30 ms = 150 ms total */
#define ANIM_TICK_MS        50    /* background matrix advances every 50 ms */

// ── Public rendering API ──────────────────────────────────────────────────────

// Draw the startup intro: matrix rain background + fading title card.
// `left` must be sized to full_rows x full_cols before calling (main.c does this).
// `right` is unused during intro but kept in signature for consistency.
// intro_frame: 0 = rain only, INTRO_FRAMES-1 = fully visible title card.
void draw_intro(struct notcurses* nc,
                struct ncplane* left, struct ncplane* right,
                unsigned int full_rows, unsigned int full_cols,
                int intro_frame);

// Draw the dual-column split-screen TUI (MODE_LIST).
// frame_tick is forwarded to the background matrix widget drawn inside left_pane.
void draw_interface(struct notcurses* nc,
                    struct ncplane* left, struct ncplane* right,
                    AppState* state, uint64_t frame_tick);

// Draw the modal product-creation form (MODE_FORM) on the standard plane.
void draw_form(struct notcurses* nc, struct ncplane* std_plane,
               const FormState* form, const AppState* state);

// Advance the matrix trace pattern by one step.
// plane  – dedicated background animation sub-plane
// frame_tick – monotonic counter driving the pattern offset
void update_background_animation(struct ncplane* plane, uint64_t frame_tick);

#endif // UI_H
