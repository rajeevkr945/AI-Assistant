#ifndef UI_H
#define UI_H

#include "models.h"
#include <stdint.h>
#include <notcurses/notcurses.h>

// ── View modes ────────────────────────────────────────────────────────────────
typedef enum {
    MODE_INTRO,     // startup ASCII art animation
    MODE_LIST,      // normal dual-pane workspace list
    MODE_FORM,      // modal product/project creation form
    MODE_CONFIRM,   // confirmation dialog (delete)
    MODE_EDIT,      // edit existing product/project name+path
} ViewMode;

// ── Form sub-state ────────────────────────────────────────────────────────────
typedef enum {
    FORM_FIELD_NAME    = 0,
    FORM_FIELD_PATH    = 1,
    FORM_FIELD_REDMINE = 2,
} FormField;

typedef enum {
    FORM_MODE_ADD_PRODUCT = 0,
    FORM_MODE_ADD_PROJECT = 1
} FormMode;

typedef struct {
    char new_name[MAX_NAME];   // Product/Project Name input buffer
    char new_path[MAX_PATH];   // Local Path input buffer (product only)
    FormField active_field;    // Which field has keyboard focus
    char error_msg[128];       // Non-empty = validation error
    int  target_project_index; // Project to add product into
    FormMode form_mode;        // Adding a product or a new project
} FormState;

// ── Confirm dialog state ──────────────────────────────────────────────────────
typedef enum {
    CONFIRM_DELETE_PRODUCT = 0,
    CONFIRM_DELETE_PROJECT = 1,
} ConfirmAction;

typedef struct {
    ConfirmAction action;
    char subject_name[MAX_NAME]; // what is being deleted (for the prompt)
    int  product_flat_index;     // flat product index (CONFIRM_DELETE_PRODUCT)
    int  project_index;          // project index     (CONFIRM_DELETE_PROJECT)
} ConfirmState;

// ── Edit state ────────────────────────────────────────────────────────────────
typedef enum {
    EDIT_TARGET_PRODUCT = 0,
    EDIT_TARGET_PROJECT = 1,
} EditTarget;

typedef struct {
    EditTarget   target;
    int          product_flat_index;  // which product (EDIT_TARGET_PRODUCT)
    int          project_index;       // which project
    char         name_buf[MAX_NAME];  // editable name
    char         path_buf[MAX_PATH];  // editable path (product only)
    FormField    active_field;
    char         error_msg[128];
} EditState;

// ── Animation state ───────────────────────────────────────────────────────────
typedef struct {
    uint64_t frame_tick;   // incremented every ANIM_TICK_MS; drives bg widget
    int      intro_frame;  // 0..INTRO_FRAMES-1 during intro
} AnimState;

// Intro animation constants
// 20 frames × 40 ms = 800 ms total intro
#define INTRO_FRAMES      20
#define INTRO_FRAME_MS    40
#define ANIM_TICK_MS      50

// ── Public rendering API ──────────────────────────────────────────────────────

void draw_intro(struct notcurses* nc,
                struct ncplane* left, struct ncplane* right,
                unsigned int full_rows, unsigned int full_cols,
                int intro_frame);

void draw_interface(struct notcurses* nc,
                    struct ncplane* left, struct ncplane* right,
                    AppState* state, uint64_t frame_tick);

void draw_form(struct notcurses* nc, struct ncplane* std_plane,
               const FormState* form, const AppState* state);

void draw_confirm(struct notcurses* nc, struct ncplane* std_plane,
                  const ConfirmState* confirm, const AppState* state);

void draw_edit(struct notcurses* nc, struct ncplane* std_plane,
               const EditState* edit, const AppState* state);

void update_background_animation(struct ncplane* plane, uint64_t frame_tick);

#endif // UI_H
