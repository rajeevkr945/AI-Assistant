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
    FORM_FIELD_PROJECT = 0,  // project name (add product)
    FORM_FIELD_NAME    = 1,  // product or project name
    FORM_FIELD_PATH    = 2,  // local path (product)
    FORM_FIELD_TASKS   = 3,
    FORM_FIELD_LINKS   = 4,
    FORM_FIELD_NOTES   = 5,
} FormField;

#define FORM_FIELD_PRODUCT_COUNT 6  // fields when adding a product
#define FORM_FIELD_PROJECT_COUNT 1  // fields when adding a project (name only)

typedef enum {
    FORM_MODE_ADD_PRODUCT = 0,
    FORM_MODE_ADD_PROJECT = 1
} FormMode;

typedef struct {
    char new_project[MAX_NAME]; // Project name (add product: pick/create project)
    char new_name[MAX_NAME];    // Product/Project Name input buffer
    char new_path[MAX_PATH];    // Local Path input buffer (product only)
    char new_tasks[MAX_TASKS];
    char new_links[MAX_LINK_LEN];
    char new_notes[MAX_NOTES];
    FormField active_field;     // Which field has keyboard focus
    char error_msg[128];        // Non-empty = validation error
    int  target_project_index;  // Pre-selected project (hint; form can override via name)
    FormMode form_mode;         // Adding a product or a new project
} FormState;

// ── Confirm dialog state ──────────────────────────────────────────────────────
typedef enum {
    CONFIRM_DELETE_PRODUCT = 0,
    CONFIRM_DELETE_PROJECT = 1,
} ConfirmAction;

typedef struct {
    ConfirmAction action;
    char subject_name[MAX_NAME]; // what is being deleted (for the prompt)
    int  product_index;          // product index within project (CONFIRM_DELETE_PRODUCT)
    int  project_index;          // project index
} ConfirmState;

// ── Edit state ────────────────────────────────────────────────────────────────
typedef enum {
    EDIT_TARGET_PRODUCT = 0,
    EDIT_TARGET_PROJECT = 1,
} EditTarget;

typedef struct {
    EditTarget   target;
    int          product_index;       // which product within project
    int          project_index;       // which project
    char         name_buf[MAX_NAME];  // editable name
    char         path_buf[MAX_PATH];  // editable path (product only)
    char         tasks_buf[MAX_TASKS];
    char         links_buf[MAX_LINK_LEN];
    char         notes_buf[MAX_NOTES];
    FormField    active_field;
    char         error_msg[128];
} EditState;

// ── Animation state ───────────────────────────────────────────────────────────
typedef struct {
    uint64_t frame_tick;   // incremented every ANIM_TICK_MS; drives bg widget
    int      intro_frame;  // 0..INTRO_FRAMES-1 during intro
} AnimState;

// Intro animation constants
// 10 frames × 28 ms = 280 ms total (< 300 ms)
#define INTRO_FRAMES      10
#define INTRO_FRAME_MS    28
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
