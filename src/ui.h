#ifndef UI_H
#define UI_H

#include "models.h"
#include <stdint.h>
#include <notcurses/notcurses.h>

// ── View modes ────────────────────────────────────────────────────────────────
typedef enum {
    MODE_INTRO,
    MODE_LIST,
    MODE_FORM,
    MODE_CONFIRM,
    MODE_EDIT,
    MODE_HELP,
    MODE_FILTER,
} ViewMode;

// ── Form fields ───────────────────────────────────────────────────────────────
typedef enum {
    // Task fields
    FORM_FIELD_PROJECT       = 0,   // project name pick/create (add-task only)
    FORM_FIELD_NAME          = 1,   // task/project name
    FORM_FIELD_PATH          = 2,   // task local path
    FORM_FIELD_OBSIDIAN      = 3,   // task obsidian doc path
    FORM_FIELD_REDMINE       = 4,   // task redmine URL
    FORM_FIELD_REMOTE_HOST   = 5,
    FORM_FIELD_REMOTE_USER   = 6,
    FORM_FIELD_REMOTE_PASS   = 7,
    FORM_FIELD_GERRIT_COMMIT = 8,
    FORM_FIELD_NOTES         = 9,
    // Project fields (used in project add/edit)
    FORM_FIELD_PROJ_DESC        = 20,
    FORM_FIELD_PROJ_GERRIT_URL  = 21,
    FORM_FIELD_PROJ_GIT_PATH    = 22,
    FORM_FIELD_PROJ_REDMINE_URL = 23,
    // Legacy — kept only so old code referring to LINKS/TASKS still compiles.
    FORM_FIELD_LINKS = 30,
    FORM_FIELD_TASKS = 31,
} FormField;

#define FORM_FIELD_TASK_COUNT 10   // task add/edit fields
#define FORM_FIELD_PROJ_COUNT 5    // project add/edit fields (name + 4 details)

typedef enum {
    FORM_MODE_ADD_PRODUCT = 0,   // add task
    FORM_MODE_ADD_PROJECT = 1
} FormMode;

typedef struct {
    char new_project[MAX_NAME];
    char new_name[MAX_NAME];
    char new_path[MAX_PATH];
    char new_obsidian[MAX_PATH];
    char new_redmine[MAX_LINK_LEN];
    char new_remote_host[MAX_PATH];
    char new_remote_user[MAX_NAME];
    char new_remote_pass[MAX_NAME];
    char new_gerrit_commit[MAX_LINK_LEN];
    char new_notes[MAX_NOTES];
    // Project-level fields
    char new_description[MAX_DESC];
    char new_gerrit_url[MAX_LINK_LEN];
    char new_git_path[MAX_PATH];
    char new_redmine_url[MAX_LINK_LEN];

    FormField active_field;
    char error_msg[128];
    int  target_project_index;
    FormMode form_mode;
} FormState;

// ── Confirm dialog state ──────────────────────────────────────────────────────
typedef enum {
    CONFIRM_DELETE_PRODUCT = 0,
    CONFIRM_DELETE_PROJECT = 1,
} ConfirmAction;

typedef struct {
    ConfirmAction action;
    char subject_name[MAX_NAME];
    int  product_index;
    int  project_index;
} ConfirmState;

// ── Edit state ────────────────────────────────────────────────────────────────
typedef enum {
    EDIT_TARGET_PRODUCT = 0,
    EDIT_TARGET_PROJECT = 1,
} EditTarget;

typedef struct {
    EditTarget target;
    int product_index;
    int project_index;

    // Task buffers
    char name_buf[MAX_NAME];
    char path_buf[MAX_PATH];
    char obsidian_buf[MAX_PATH];
    char redmine_buf[MAX_LINK_LEN];
    char remote_host_buf[MAX_PATH];
    char remote_user_buf[MAX_NAME];
    char remote_pass_buf[MAX_NAME];
    char gerrit_commit_buf[MAX_LINK_LEN];
    char notes_buf[MAX_NOTES];
    // Project buffers
    char description_buf[MAX_DESC];
    char gerrit_url_buf[MAX_LINK_LEN];
    char git_path_buf[MAX_PATH];
    char redmine_url_buf[MAX_LINK_LEN];

    FormField active_field;
    char error_msg[128];
} EditState;

// ── Help / Filter state ───────────────────────────────────────────────────────
typedef struct {
    int scroll_offset;
} HelpState;

typedef struct {
    char query[64];
} FilterState;

// ── Hit map (mouse) ───────────────────────────────────────────────────────────
// Recorded by draw_interface each frame so main can convert mouse (y,x)
// to actions. Zero-inited = no hit target.
typedef struct {
    int  obsidian_y;
    int  obsidian_x;
    int  obsidian_w;
    char obsidian_target[MAX_PATH];
} HitMap;

// ── Animation state ───────────────────────────────────────────────────────────
typedef struct {
    uint64_t frame_tick;
    int      intro_frame;
} AnimState;

#define INTRO_FRAMES      25
#define INTRO_FRAME_MS    28
#define ANIM_TICK_MS      50

// ── Public rendering API ──────────────────────────────────────────────────────
void draw_intro(struct notcurses* nc,
                struct ncplane* left, struct ncplane* right,
                unsigned int full_rows, unsigned int full_cols,
                int intro_frame);

void draw_interface(struct notcurses* nc,
                    struct ncplane* left, struct ncplane* right,
                    AppState* state, uint64_t frame_tick,
                    const FilterState* filter, HitMap* out_hit);

void draw_form(struct notcurses* nc, struct ncplane* std_plane,
               const FormState* form, const AppState* state);

void draw_confirm(struct notcurses* nc, struct ncplane* std_plane,
                  const ConfirmState* confirm, const AppState* state);

void draw_edit(struct notcurses* nc, struct ncplane* std_plane,
               const EditState* edit, const AppState* state);

void draw_help(struct notcurses* nc, struct ncplane* std_plane,
               const HelpState* help);

void draw_filter_overlay(struct notcurses* nc, struct ncplane* std_plane,
                         const FilterState* filter);

void update_background_animation(struct ncplane* plane, uint64_t frame_tick);

// Populate cache->git_graph_cache by running `git -C <path> log --graph ...`.
// Safe to call with an empty/nonexistent path (writes empty output).
void refresh_git_graph(AppState *state);

#endif // UI_H
