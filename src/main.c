#define _POSIX_C_SOURCE 200809L

#include "models.h"
#include "ipc.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <notcurses/notcurses.h>

// ─────────────────────────────────────────────────────────────────────────────
// Time helpers
// ─────────────────────────────────────────────────────────────────────────────
static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}
static void sleep_ms(long ms) {
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

// ─────────────────────────────────────────────────────────────────────────────
// Input buffer helpers (printable ASCII only)
// ─────────────────────────────────────────────────────────────────────────────
static int buf_append(char *buf, size_t max_len, uint32_t key) {
    if (key < 0x20 || key > 0x7E) return 0;
    size_t len = strlen(buf);
    if (len + 1 >= max_len) return 0;
    buf[len]     = (char)key;
    buf[len + 1] = '\0';
    return 1;
}
static void buf_backspace(char *buf) {
    size_t len = strlen(buf);
    if (len > 0) buf[len - 1] = '\0';
}

static char *form_field_buf(FormState *form) {
    switch (form->active_field) {
        case FORM_FIELD_PROJECT: return form->new_project;
        case FORM_FIELD_NAME:    return form->new_name;
        case FORM_FIELD_PATH:    return form->new_path;
        case FORM_FIELD_TASKS:   return form->new_tasks;
        case FORM_FIELD_LINKS:   return form->new_links;
        case FORM_FIELD_NOTES:   return form->new_notes;
        default:                 return form->new_name;
    }
}
static size_t form_field_max(FormState *form) {
    switch (form->active_field) {
        case FORM_FIELD_PROJECT: return MAX_NAME;
        case FORM_FIELD_NAME:    return MAX_NAME;
        case FORM_FIELD_PATH:    return MAX_PATH;
        case FORM_FIELD_TASKS:   return MAX_TASKS;
        case FORM_FIELD_LINKS:   return MAX_LINK_LEN;
        case FORM_FIELD_NOTES:   return MAX_NOTES;
        default:                 return MAX_NAME;
    }
}

static char *edit_field_buf(EditState *edit) {
    switch (edit->active_field) {
        case FORM_FIELD_NAME:  return edit->name_buf;
        case FORM_FIELD_PATH:  return edit->path_buf;
        case FORM_FIELD_TASKS: return edit->tasks_buf;
        case FORM_FIELD_LINKS: return edit->links_buf;
        case FORM_FIELD_NOTES: return edit->notes_buf;
        default:               return edit->name_buf;
    }
}
static size_t edit_field_max(EditState *edit) {
    switch (edit->active_field) {
        case FORM_FIELD_NAME:  return MAX_NAME;
        case FORM_FIELD_PATH:  return MAX_PATH;
        case FORM_FIELD_TASKS: return MAX_TASKS;
        case FORM_FIELD_LINKS: return MAX_LINK_LEN;
        case FORM_FIELD_NOTES: return MAX_NOTES;
        default:               return MAX_NAME;
    }
}

// Cycle product-form fields
static FormField next_product_field(FormField f, int dir) {
    int v = (int)f + dir;
    if (v < 0) v = FORM_FIELD_PRODUCT_COUNT - 1;
    if (v >= FORM_FIELD_PRODUCT_COUNT) v = 0;
    return (FormField)v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pane helpers
// ─────────────────────────────────────────────────────────────────────────────
static void hide_panes(struct ncplane* left, struct ncplane* right,
                       unsigned int rows) {
    ncplane_resize_simple(left,  1, 1);
    ncplane_move_yx(left,  (int)rows, 0);
    ncplane_resize_simple(right, 1, 1);
    ncplane_move_yx(right, (int)rows, 0);
}
static void restore_panes(struct ncplane* left, struct ncplane* right,
                          unsigned int rows, unsigned int left_cols,
                          unsigned int right_cols) {
    ncplane_resize_simple(left,  rows - 3, left_cols);
    ncplane_move_yx(left,  2, 0);
    ncplane_resize_simple(right, rows - 3, right_cols);
    ncplane_move_yx(right, 2, (int)left_cols);
}

// ─────────────────────────────────────────────────────────────────────────────
// Delete helpers
// ─────────────────────────────────────────────────────────────────────────────
static int delete_product_in_project(AppState *state, int project_index, int product_index) {
    if (project_index < 0 || project_index >= state->project_count) return -1;
    Project *proj = &state->projects[project_index];
    if (product_index < 0 || product_index >= proj->product_count) return -1;
    for (int k = product_index; k < proj->product_count - 1; k++)
        proj->products[k] = proj->products[k + 1];
    proj->product_count--;
    return 0;
}

static int delete_project(AppState *state, int project_index) {
    if (project_index < 0 || project_index >= state->project_count) return -1;
    for (int i = project_index; i < state->project_count - 1; i++)
        state->projects[i] = state->projects[i + 1];
    state->project_count--;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char **argv) {
    static AppState state;
    if (load_config(&state) != 0) {
        fprintf(stderr, "Error: Failed to load ~/.subash.conf\n");
        return 1;
    }
    normalize_selection(&state);

    // ── Headless / CLI mode ───────────────────────────────────────────────────
    if (argc > 1) {
        const char *target = argv[1];
        const char *found_path = NULL, *found_name = NULL;
        bool is_project = false;

        for (int i = 0; i < state.project_count && !found_path; i++) {
            Project *proj = &state.projects[i];
            for (int j = 0; j < proj->product_count; j++) {
                if (strcmp(proj->products[j].name, target) == 0) {
                    found_path = proj->products[j].local_path;
                    found_name = proj->products[j].name;
                    break;
                }
            }
        }
        if (!found_path) {
            for (int i = 0; i < state.project_count; i++) {
                if (strcmp(state.projects[i].name, target) == 0 &&
                    state.projects[i].product_count > 0) {
                    found_path = state.projects[i].products[0].local_path;
                    found_name = state.projects[i].name;
                    is_project = true;
                    break;
                }
            }
        }
        if (found_path) {
            if (trigger_bash_action(found_path) == 0) {
                printf("Jumped to %s '%s' -> %s\n",
                       is_project ? "Project" : "Product", found_name, found_path);
                return 0;
            }
            fprintf(stderr, "Error: Failed to write IPC trigger.\n");
            return 1;
        }
        fprintf(stderr, "Error: No project or product matching '%s'\n", target);
        return 1;
    }

    // ── TUI mode ──────────────────────────────────────────────────────────────
    setlocale(LC_ALL, "");

    struct notcurses_options nopts = {
        .flags = NCOPTION_INHIBIT_SETLOCALE | NCOPTION_SUPPRESS_BANNERS
    };
    struct notcurses *nc = notcurses_init(&nopts, NULL);
    if (!nc) { fprintf(stderr, "Error: Failed to init notcurses.\n"); return 1; }

    struct ncplane *stdplane = notcurses_stdplane(nc);
    unsigned int rows, cols;
    ncplane_dim_yx(stdplane, &rows, &cols);

    // Narrower project list column (~16% instead of 28%)
    unsigned int left_cols  = cols * 16 / 100;
    if (left_cols < 14) left_cols = (cols > 14) ? 14 : cols / 4;
    if (left_cols > 22) left_cols = 22;
    unsigned int right_cols = cols - left_cols;

    struct ncplane_options lo = {
        .y = 2, .x = 0, .rows = rows - 3, .cols = left_cols,
        .name = "left_pane", .flags = 0,
    };
    struct ncplane *left_pane = ncplane_create(stdplane, &lo);

    struct ncplane_options ro = {
        .y = 2, .x = (int)left_cols, .rows = rows - 3, .cols = right_cols,
        .name = "right_pane", .flags = 0,
    };
    struct ncplane *right_pane = ncplane_create(stdplane, &ro);

    ViewMode view_mode = MODE_INTRO;
    AnimState anim = { .frame_tick = 0, .intro_frame = 0 };

    FormState form;
    memset(&form, 0, sizeof(FormState));
    form.active_field = FORM_FIELD_PROJECT;

    ConfirmState confirm;
    memset(&confirm, 0, sizeof(ConfirmState));

    EditState edit;
    memset(&edit, 0, sizeof(EditState));

    // Expand left_pane to full screen for intro
    ncplane_resize_simple(left_pane, rows, cols);
    ncplane_move_yx(left_pane, 0, 0);
    ncplane_resize_simple(right_pane, 1, 1);
    ncplane_move_yx(right_pane, (int)rows, 0);

    uint64_t last_anim_ms  = now_ms();
    uint64_t last_intro_ms = now_ms();

    ncinput ni;

    while (!state.should_quit) {
        uint64_t current_ms = now_ms();

        if (view_mode == MODE_INTRO) {
            if (current_ms - last_intro_ms >= INTRO_FRAME_MS) {
                last_intro_ms = current_ms;
                anim.intro_frame++;
                if (anim.intro_frame >= INTRO_FRAMES) {
                    restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                    view_mode = MODE_LIST;
                }
            }
        }
        if (current_ms - last_anim_ms >= ANIM_TICK_MS) {
            last_anim_ms = current_ms;
            anim.frame_tick++;
        }

        switch (view_mode) {
            case MODE_INTRO:
                draw_intro(nc, left_pane, right_pane, rows, cols, anim.intro_frame);
                break;
            case MODE_LIST:
                draw_interface(nc, left_pane, right_pane, &state, anim.frame_tick);
                break;
            case MODE_FORM:
                draw_form(nc, stdplane, &form, &state);
                break;
            case MODE_CONFIRM:
                draw_confirm(nc, stdplane, &confirm, &state);
                break;
            case MODE_EDIT:
                draw_edit(nc, stdplane, &edit, &state);
                break;
        }
        notcurses_render(nc);

        uint32_t key = notcurses_get_nblock(nc, &ni);
        if (key == (uint32_t)-1) break;
        if (key == 0) { sleep_ms(5); continue; }

        if (key == NCKEY_RESIZE) {
            ncplane_dim_yx(stdplane, &rows, &cols);
            left_cols  = cols * 16 / 100;
            if (left_cols < 14) left_cols = (cols > 14) ? 14 : cols / 4;
            if (left_cols > 22) left_cols = 22;
            right_cols = cols - left_cols;
            if (view_mode == MODE_LIST) {
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
            } else if (view_mode == MODE_INTRO) {
                ncplane_resize_simple(left_pane, rows, cols);
                ncplane_move_yx(left_pane, 0, 0);
            }
            continue;
        }

        if (view_mode == MODE_INTRO) continue;

        // =====================================================================
        // MODE_LIST
        // =====================================================================
        if (view_mode == MODE_LIST) {

            if (key == 'q' || key == 'Q' || key == NCKEY_ESC) {
                state.should_quit = true;

            // ── Left / Right: switch project ↔ product focus ─────────────────
            } else if (key == NCKEY_LEFT || key == 'h' || key == 'H') {
                state.nav_focus = NAV_FOCUS_PROJECT;

            } else if (key == NCKEY_RIGHT || key == 'l' || key == 'L') {
                state.nav_focus = NAV_FOCUS_PRODUCT;
                normalize_selection(&state);

            } else if (key == 'j' || key == 'J' || key == NCKEY_DOWN) {
                if (state.nav_focus == NAV_FOCUS_PROJECT) {
                    if (state.project_count > 0) {
                        state.selected_project =
                            (state.selected_project + 1) % state.project_count;
                        state.selected_product = 0;
                        normalize_selection(&state);
                    }
                } else {
                    if (state.project_count > 0) {
                        Project *p = &state.projects[state.selected_project];
                        if (p->product_count > 0)
                            state.selected_product =
                                (state.selected_product + 1) % p->product_count;
                    }
                }

            } else if (key == 'k' || key == 'K' || key == NCKEY_UP) {
                if (state.nav_focus == NAV_FOCUS_PROJECT) {
                    if (state.project_count > 0) {
                        state.selected_project =
                            state.selected_project > 0
                                ? state.selected_project - 1
                                : state.project_count - 1;
                        state.selected_product = 0;
                        normalize_selection(&state);
                    }
                } else {
                    if (state.project_count > 0) {
                        Project *p = &state.projects[state.selected_project];
                        if (p->product_count > 0)
                            state.selected_product =
                                state.selected_product > 0
                                    ? state.selected_product - 1
                                    : p->product_count - 1;
                    }
                }

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                Product *prod = get_selected_product(&state, NULL);
                if (prod && prod->local_path[0] &&
                    trigger_bash_action(prod->local_path) == 0) {
                    save_config(&state);
                    state.should_quit = true;
                }

            // ── Add product ──────────────────────────────────────────────────
            } else if (key == 'a' || key == 'A') {
                memset(&form, 0, sizeof(FormState));
                form.form_mode    = FORM_MODE_ADD_PRODUCT;
                form.active_field = FORM_FIELD_PROJECT;
                form.target_project_index = state.selected_project;
                // Pre-fill project name from current selection
                if (state.selected_project >= 0 &&
                    state.selected_project < state.project_count) {
                    strncpy(form.new_project,
                            state.projects[state.selected_project].name,
                            MAX_NAME - 1);
                }
                hide_panes(left_pane, right_pane, rows);
                view_mode = MODE_FORM;

            // ── New project ──────────────────────────────────────────────────
            } else if (key == 'n' || key == 'N') {
                memset(&form, 0, sizeof(FormState));
                form.active_field = FORM_FIELD_NAME;
                form.form_mode    = FORM_MODE_ADD_PROJECT;
                hide_panes(left_pane, right_pane, rows);
                view_mode = MODE_FORM;

            // ── Edit selected product ────────────────────────────────────────
            } else if (key == 'e' || key == 'E') {
                Product *eprod = get_selected_product(&state, NULL);
                if (eprod) {
                    memset(&edit, 0, sizeof(EditState));
                    edit.target         = EDIT_TARGET_PRODUCT;
                    edit.project_index  = state.selected_project;
                    edit.product_index  = state.selected_product;
                    edit.active_field   = FORM_FIELD_NAME;
                    strncpy(edit.name_buf,  eprod->name,       MAX_NAME - 1);
                    strncpy(edit.path_buf,  eprod->local_path, MAX_PATH - 1);
                    strncpy(edit.tasks_buf, eprod->tasks,      MAX_TASKS - 1);
                    strncpy(edit.notes_buf, eprod->notes,      MAX_NOTES - 1);
                    if (eprod->link_count > 0)
                        strncpy(edit.links_buf, eprod->links[0], MAX_LINK_LEN - 1);
                    hide_panes(left_pane, right_pane, rows);
                    view_mode = MODE_EDIT;
                }

            // ── Edit active project (Shift-P) ────────────────────────────────
            } else if (key == 'P') {
                if (state.selected_project >= 0 &&
                    state.selected_project < state.project_count) {
                    Project *ep = &state.projects[state.selected_project];
                    memset(&edit, 0, sizeof(EditState));
                    edit.target        = EDIT_TARGET_PROJECT;
                    edit.project_index = state.selected_project;
                    edit.active_field  = FORM_FIELD_NAME;
                    strncpy(edit.name_buf, ep->name, MAX_NAME - 1);
                    hide_panes(left_pane, right_pane, rows);
                    view_mode = MODE_EDIT;
                }

            // ── Delete product ───────────────────────────────────────────────
            } else if (key == 'd') {
                Product *dprod = get_selected_product(&state, NULL);
                if (dprod) {
                    memset(&confirm, 0, sizeof(ConfirmState));
                    confirm.action         = CONFIRM_DELETE_PRODUCT;
                    confirm.project_index  = state.selected_project;
                    confirm.product_index  = state.selected_product;
                    strncpy(confirm.subject_name, dprod->name, MAX_NAME - 1);
                    hide_panes(left_pane, right_pane, rows);
                    view_mode = MODE_CONFIRM;
                }

            // ── Delete project ───────────────────────────────────────────────
            } else if (key == 'D') {
                if (state.selected_project >= 0 &&
                    state.selected_project < state.project_count) {
                    Project *dp = &state.projects[state.selected_project];
                    memset(&confirm, 0, sizeof(ConfirmState));
                    confirm.action        = CONFIRM_DELETE_PROJECT;
                    confirm.project_index = state.selected_project;
                    strncpy(confirm.subject_name, dp->name, MAX_NAME - 1);
                    hide_panes(left_pane, right_pane, rows);
                    view_mode = MODE_CONFIRM;
                }
            }

        // =====================================================================
        // MODE_CONFIRM
        // =====================================================================
        } else if (view_mode == MODE_CONFIRM) {
            if (key == 'y' || key == 'Y') {
                if (confirm.action == CONFIRM_DELETE_PRODUCT) {
                    delete_product_in_project(&state, confirm.project_index,
                                              confirm.product_index);
                } else {
                    delete_project(&state, confirm.project_index);
                }
                normalize_selection(&state);
                save_config(&state);
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                view_mode = MODE_LIST;

            } else if (key == 'n' || key == 'N' || key == NCKEY_ESC) {
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                view_mode = MODE_LIST;
            }

        // =====================================================================
        // MODE_EDIT
        // =====================================================================
        } else if (view_mode == MODE_EDIT) {
            if (key == NCKEY_ESC) {
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                view_mode = MODE_LIST;

            } else if (key == NCKEY_BACKSPACE || key == 127) {
                buf_backspace(edit_field_buf(&edit));
                edit.error_msg[0] = '\0';

            } else if ((key == NCKEY_TAB || key == NCKEY_DOWN)
                       && edit.target == EDIT_TARGET_PRODUCT) {
                // cycle: NAME → PATH → TASKS → LINKS → NOTES → NAME
                static const FormField cycle[] = {
                    FORM_FIELD_NAME, FORM_FIELD_PATH, FORM_FIELD_TASKS,
                    FORM_FIELD_LINKS, FORM_FIELD_NOTES
                };
                int idx = 0;
                for (int i = 0; i < 5; i++)
                    if (cycle[i] == edit.active_field) { idx = i; break; }
                edit.active_field = cycle[(idx + 1) % 5];
                edit.error_msg[0] = '\0';

            } else if (key == NCKEY_UP && edit.target == EDIT_TARGET_PRODUCT) {
                static const FormField cycle[] = {
                    FORM_FIELD_NAME, FORM_FIELD_PATH, FORM_FIELD_TASKS,
                    FORM_FIELD_LINKS, FORM_FIELD_NOTES
                };
                int idx = 0;
                for (int i = 0; i < 5; i++)
                    if (cycle[i] == edit.active_field) { idx = i; break; }
                edit.active_field = cycle[(idx + 4) % 5];
                edit.error_msg[0] = '\0';

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                if (edit.target == EDIT_TARGET_PRODUCT) {
                    if (edit.name_buf[0] == '\0') {
                        snprintf(edit.error_msg, sizeof(edit.error_msg),
                                 "Name cannot be empty.");
                        edit.active_field = FORM_FIELD_NAME;
                    } else {
                        if (edit.project_index >= 0 &&
                            edit.project_index < state.project_count) {
                            Project *proj = &state.projects[edit.project_index];
                            if (edit.product_index >= 0 &&
                                edit.product_index < proj->product_count) {
                                Product *p = &proj->products[edit.product_index];
                                strncpy(p->name,       edit.name_buf,  MAX_NAME - 1);
                                strncpy(p->local_path, edit.path_buf,  MAX_PATH - 1);
                                strncpy(p->tasks,      edit.tasks_buf, MAX_TASKS - 1);
                                strncpy(p->notes,      edit.notes_buf, MAX_NOTES - 1);
                                p->name[MAX_NAME - 1]       = '\0';
                                p->local_path[MAX_PATH - 1] = '\0';
                                p->tasks[MAX_TASKS - 1]     = '\0';
                                p->notes[MAX_NOTES - 1]     = '\0';
                                if (edit.links_buf[0] != '\0') {
                                    strncpy(p->links[0], edit.links_buf, MAX_LINK_LEN - 1);
                                    p->links[0][MAX_LINK_LEN - 1] = '\0';
                                    if (p->link_count < 1) p->link_count = 1;
                                }
                            }
                        }
                        save_config(&state);
                        restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                        view_mode = MODE_LIST;
                    }
                } else {
                    if (edit.name_buf[0] == '\0') {
                        snprintf(edit.error_msg, sizeof(edit.error_msg),
                                 "Name cannot be empty.");
                    } else {
                        Project *proj = &state.projects[edit.project_index];
                        strncpy(proj->name, edit.name_buf, MAX_NAME - 1);
                        proj->name[MAX_NAME - 1] = '\0';
                        save_config(&state);
                        restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                        view_mode = MODE_LIST;
                    }
                }

            } else {
                buf_append(edit_field_buf(&edit), edit_field_max(&edit), key);
                edit.error_msg[0] = '\0';
            }

        // =====================================================================
        // MODE_FORM
        // =====================================================================
        } else if (view_mode == MODE_FORM) {
            if (key == NCKEY_ESC) {
                memset(&form, 0, sizeof(FormState));
                form.active_field = FORM_FIELD_PROJECT;
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                view_mode = MODE_LIST;

            } else if (key == NCKEY_BACKSPACE || key == 127) {
                if (form.form_mode == FORM_MODE_ADD_PROJECT)
                    buf_backspace(form.new_name);
                else
                    buf_backspace(form_field_buf(&form));
                form.error_msg[0] = '\0';

            } else if ((key == NCKEY_TAB || key == NCKEY_DOWN)
                       && form.form_mode == FORM_MODE_ADD_PRODUCT) {
                form.active_field = next_product_field(form.active_field, +1);
                form.error_msg[0] = '\0';

            } else if (key == NCKEY_UP && form.form_mode == FORM_MODE_ADD_PRODUCT) {
                form.active_field = next_product_field(form.active_field, -1);
                form.error_msg[0] = '\0';

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                if (form.form_mode == FORM_MODE_ADD_PROJECT) {
                    if (form.new_name[0] == '\0') {
                        snprintf(form.error_msg, sizeof(form.error_msg),
                                 "Name cannot be empty.");
                    } else if (state.project_count >= MAX_PROJECTS) {
                        snprintf(form.error_msg, sizeof(form.error_msg),
                                 "Too many projects (max %d).", MAX_PROJECTS);
                    } else if (find_project_by_name(&state, form.new_name) >= 0) {
                        snprintf(form.error_msg, sizeof(form.error_msg),
                                 "Project already exists.");
                    } else {
                        Project *np = &state.projects[state.project_count];
                        memset(np, 0, sizeof(Project));
                        strncpy(np->name, form.new_name, MAX_NAME - 1);
                        np->name[MAX_NAME - 1] = '\0';
                        state.project_count++;
                        state.selected_project = state.project_count - 1;
                        state.selected_product = 0;
                        save_config(&state);
                        memset(&form, 0, sizeof(FormState));
                        restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                        view_mode = MODE_LIST;
                    }
                } else {
                    // add product: advance fields until last, then save
                    if (form.active_field != FORM_FIELD_NOTES) {
                        // validate required fields as we leave them
                        if (form.active_field == FORM_FIELD_PROJECT &&
                            form.new_project[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg),
                                     "Project name cannot be empty.");
                        } else if (form.active_field == FORM_FIELD_NAME &&
                                   form.new_name[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg),
                                     "Product name cannot be empty.");
                        } else {
                            form.active_field = next_product_field(form.active_field, +1);
                            form.error_msg[0] = '\0';
                        }
                    } else {
                        // final save
                        if (form.new_project[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg),
                                     "Project name cannot be empty.");
                            form.active_field = FORM_FIELD_PROJECT;
                        } else if (form.new_name[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg),
                                     "Product name cannot be empty.");
                            form.active_field = FORM_FIELD_NAME;
                        } else {
                            int pidx = ensure_project(&state, form.new_project);
                            if (pidx < 0) {
                                snprintf(form.error_msg, sizeof(form.error_msg),
                                         "Too many projects (max %d).", MAX_PROJECTS);
                            } else if (add_product_to_state(&state, pidx,
                                       form.new_name, form.new_path,
                                       form.new_tasks, form.new_links,
                                       form.new_notes) == 0) {
                                state.selected_project = pidx;
                                state.selected_product =
                                    state.projects[pidx].product_count - 1;
                                state.nav_focus = NAV_FOCUS_PRODUCT;
                                save_config(&state);
                                memset(&form, 0, sizeof(FormState));
                                restore_panes(left_pane, right_pane, rows,
                                              left_cols, right_cols);
                                view_mode = MODE_LIST;
                            } else {
                                snprintf(form.error_msg, sizeof(form.error_msg),
                                         "Project full (max %d products).", MAX_PRODUCTS);
                            }
                        }
                    }
                }

            } else {
                if (form.form_mode == FORM_MODE_ADD_PROJECT)
                    buf_append(form.new_name, sizeof(form.new_name), key);
                else
                    buf_append(form_field_buf(&form), form_field_max(&form), key);
                form.error_msg[0] = '\0';
            }
        }
    }

    notcurses_stop(nc);
    return 0;
}
