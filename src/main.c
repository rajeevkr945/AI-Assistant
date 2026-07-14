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

// ─────────────────────────────────────────────────────────────────────────────
// Pane helpers: hide / restore split layout
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

// Remove the product at flat_index from state. Returns 0 on success.
static int delete_product(AppState *state, int flat_index) {
    int cur = 0;
    for (int i = 0; i < state->project_count; i++) {
        Project *proj = &state->projects[i];
        for (int j = 0; j < proj->product_count; j++) {
            if (cur == flat_index) {
                // shift products left
                for (int k = j; k < proj->product_count - 1; k++)
                    proj->products[k] = proj->products[k + 1];
                proj->product_count--;
                return 0;
            }
            cur++;
        }
    }
    return -1;
}

// Remove the project at project_index from state. Returns 0 on success.
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
    AppState state;
    if (load_config(&state) != 0) {
        fprintf(stderr, "Error: Failed to load ~/.subash.conf\n");
        return 1;
    }

    int total_prods = get_total_product_count(&state);
    if (total_prods > 0) {
        if (state.selected_index >= total_prods || state.selected_index < 0)
            state.selected_index = 0;
    } else {
        state.selected_index = 0;
    }

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

    unsigned int left_cols  = cols * 28 / 100;
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

    // ── App state ─────────────────────────────────────────────────────────────
    ViewMode view_mode = MODE_INTRO;

    AnimState anim = { .frame_tick = 0, .intro_frame = 0 };

    FormState form;
    memset(&form, 0, sizeof(FormState));
    form.active_field = FORM_FIELD_NAME;

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

        // ── Time-based events ─────────────────────────────────────────────────
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

        // ── Render ────────────────────────────────────────────────────────────
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

        // ── Non-blocking input ────────────────────────────────────────────────
        uint32_t key = notcurses_get_nblock(nc, &ni);
        if (key == (uint32_t)-1) break;
        if (key == 0) { sleep_ms(5); continue; }

        // resize — all modes
        if (key == NCKEY_RESIZE) {
            ncplane_dim_yx(stdplane, &rows, &cols);
            left_cols  = cols * 28 / 100;
            right_cols = cols - left_cols;
            if (view_mode == MODE_LIST) {
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
            } else if (view_mode == MODE_INTRO) {
                ncplane_resize_simple(left_pane, rows, cols);
                ncplane_move_yx(left_pane, 0, 0);
            }
            continue;
        }

        if (view_mode == MODE_INTRO) continue; // swallow during intro

        // =====================================================================
        // MODE_LIST
        // =====================================================================
        if (view_mode == MODE_LIST) {
            int total = get_total_product_count(&state);

            if (key == 'q' || key == 'Q' || key == NCKEY_ESC) {
                state.should_quit = true;

            } else if (key == 'j' || key == 'J' || key == NCKEY_DOWN) {
                if (total > 0)
                    state.selected_index = (state.selected_index + 1) % total;

            } else if (key == 'k' || key == 'K' || key == NCKEY_UP) {
                if (total > 0)
                    state.selected_index = state.selected_index > 0
                                           ? state.selected_index - 1 : total - 1;

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                const Project *sp = NULL;
                Product *prod = get_product_at(&state, state.selected_index, &sp);
                if (prod && trigger_bash_action(prod->local_path) == 0) {
                    save_config(&state);
                    state.should_quit = true;
                }

            // ── Add product ──────────────────────────────────────────────────
            } else if (key == 'a' || key == 'A') {
                if (state.project_count > 0) {
                    memset(&form, 0, sizeof(FormState));
                    form.active_field = FORM_FIELD_NAME;
                    form.form_mode    = FORM_MODE_ADD_PRODUCT;
                    const Project *cp = NULL;
                    get_product_at(&state, state.selected_index, &cp);
                    form.target_project_index = 0;
                    if (cp) {
                        for (int pi = 0; pi < state.project_count; pi++) {
                            if (&state.projects[pi] == cp) {
                                form.target_project_index = pi;
                                break;
                            }
                        }
                    }
                    hide_panes(left_pane, right_pane, rows);
                    view_mode = MODE_FORM;
                }

            // ── New project ──────────────────────────────────────────────────
            } else if (key == 'n' || key == 'N') {
                memset(&form, 0, sizeof(FormState));
                form.active_field = FORM_FIELD_NAME;
                form.form_mode    = FORM_MODE_ADD_PROJECT;
                hide_panes(left_pane, right_pane, rows);
                view_mode = MODE_FORM;

            // ── Edit selected product ────────────────────────────────────────
            } else if (key == 'e' || key == 'E') {
                if (total > 0) {
                    const Project *ep = NULL;
                    Product *eprod = get_product_at(&state, state.selected_index, &ep);
                    if (eprod) {
                        memset(&edit, 0, sizeof(EditState));
                        edit.target            = EDIT_TARGET_PRODUCT;
                        edit.product_flat_index = state.selected_index;
                        edit.active_field      = FORM_FIELD_NAME;
                        strncpy(edit.name_buf, eprod->name,       MAX_NAME - 1);
                        strncpy(edit.path_buf, eprod->local_path, MAX_PATH - 1);
                        // find project index
                        edit.project_index = 0;
                        if (ep) {
                            for (int pi = 0; pi < state.project_count; pi++) {
                                if (&state.projects[pi] == ep) {
                                    edit.project_index = pi;
                                    break;
                                }
                            }
                        }
                        hide_panes(left_pane, right_pane, rows);
                        view_mode = MODE_EDIT;
                    }
                }

            // ── Edit active project (Shift-E) ────────────────────────────────
            } else if (key == 'P') {
                // 'P' = edit the project that owns the selected product
                const Project *ep = NULL;
                get_product_at(&state, state.selected_index, &ep);
                if (ep) {
                    memset(&edit, 0, sizeof(EditState));
                    edit.target       = EDIT_TARGET_PROJECT;
                    edit.active_field = FORM_FIELD_NAME;
                    for (int pi = 0; pi < state.project_count; pi++) {
                        if (&state.projects[pi] == ep) {
                            edit.project_index = pi;
                            break;
                        }
                    }
                    strncpy(edit.name_buf, ep->name, MAX_NAME - 1);
                    hide_panes(left_pane, right_pane, rows);
                    view_mode = MODE_EDIT;
                }

            // ── Delete product ───────────────────────────────────────────────
            } else if (key == 'd') {
                if (total > 0) {
                    const Project *dp = NULL;
                    Product *dprod = get_product_at(&state, state.selected_index, &dp);
                    if (dprod) {
                        memset(&confirm, 0, sizeof(ConfirmState));
                        confirm.action            = CONFIRM_DELETE_PRODUCT;
                        confirm.product_flat_index = state.selected_index;
                        strncpy(confirm.subject_name, dprod->name, MAX_NAME - 1);
                        hide_panes(left_pane, right_pane, rows);
                        view_mode = MODE_CONFIRM;
                    }
                }

            // ── Delete project ───────────────────────────────────────────────
            } else if (key == 'D') {
                const Project *dp = NULL;
                get_product_at(&state, state.selected_index, &dp);
                if (dp) {
                    memset(&confirm, 0, sizeof(ConfirmState));
                    confirm.action = CONFIRM_DELETE_PROJECT;
                    strncpy(confirm.subject_name, dp->name, MAX_NAME - 1);
                    for (int pi = 0; pi < state.project_count; pi++) {
                        if (&state.projects[pi] == dp) {
                            confirm.project_index = pi;
                            break;
                        }
                    }
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
                    delete_product(&state, confirm.product_flat_index);
                    int new_total = get_total_product_count(&state);
                    if (state.selected_index >= new_total)
                        state.selected_index = new_total > 0 ? new_total - 1 : 0;
                } else {
                    delete_project(&state, confirm.project_index);
                    int new_total = get_total_product_count(&state);
                    if (state.selected_index >= new_total)
                        state.selected_index = new_total > 0 ? new_total - 1 : 0;
                }
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
                if (edit.active_field == FORM_FIELD_NAME)
                    buf_backspace(edit.name_buf);
                else
                    buf_backspace(edit.path_buf);
                edit.error_msg[0] = '\0';

            } else if ((key == NCKEY_TAB || key == NCKEY_DOWN || key == NCKEY_UP)
                       && edit.target == EDIT_TARGET_PRODUCT) {
                edit.active_field = (edit.active_field == FORM_FIELD_NAME)
                                    ? FORM_FIELD_PATH : FORM_FIELD_NAME;
                edit.error_msg[0] = '\0';

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                if (edit.target == EDIT_TARGET_PRODUCT) {
                    if (edit.active_field == FORM_FIELD_NAME && edit.target == EDIT_TARGET_PRODUCT) {
                        // advance to path field on first Enter
                        edit.active_field = FORM_FIELD_PATH;
                        edit.error_msg[0] = '\0';
                    } else {
                        if (edit.name_buf[0] == '\0') {
                            snprintf(edit.error_msg, sizeof(edit.error_msg), "Name cannot be empty.");
                            edit.active_field = FORM_FIELD_NAME;
                        } else {
                            // apply edits
                            Product *p = get_product_at(&state, edit.product_flat_index, NULL);
                            if (p) {
                                strncpy(p->name,       edit.name_buf, MAX_NAME - 1);
                                strncpy(p->local_path, edit.path_buf, MAX_PATH - 1);
                                p->name[MAX_NAME - 1]       = '\0';
                                p->local_path[MAX_PATH - 1] = '\0';
                            }
                            save_config(&state);
                            restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                            view_mode = MODE_LIST;
                        }
                    }
                } else {
                    // edit project name
                    if (edit.name_buf[0] == '\0') {
                        snprintf(edit.error_msg, sizeof(edit.error_msg), "Name cannot be empty.");
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
                if (edit.active_field == FORM_FIELD_NAME)
                    buf_append(edit.name_buf, sizeof(edit.name_buf), key);
                else if (edit.target == EDIT_TARGET_PRODUCT)
                    buf_append(edit.path_buf, sizeof(edit.path_buf), key);
                edit.error_msg[0] = '\0';
            }

        // =====================================================================
        // MODE_FORM
        // =====================================================================
        } else if (view_mode == MODE_FORM) {
            if (key == NCKEY_ESC) {
                memset(&form, 0, sizeof(FormState));
                form.active_field = FORM_FIELD_NAME;
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                view_mode = MODE_LIST;

            } else if (key == NCKEY_BACKSPACE || key == 127) {
                if (form.active_field == FORM_FIELD_NAME)
                    buf_backspace(form.new_name);
                else
                    buf_backspace(form.new_path);
                form.error_msg[0] = '\0';

            } else if ((key == NCKEY_TAB || key == NCKEY_DOWN || key == NCKEY_UP)
                       && form.form_mode == FORM_MODE_ADD_PRODUCT) {
                form.active_field = (form.active_field == FORM_FIELD_NAME)
                                    ? FORM_FIELD_PATH : FORM_FIELD_NAME;
                form.error_msg[0] = '\0';

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                if (form.form_mode == FORM_MODE_ADD_PROJECT) {
                    if (form.new_name[0] == '\0') {
                        snprintf(form.error_msg, sizeof(form.error_msg), "Name cannot be empty.");
                    } else if (state.project_count >= MAX_PROJECTS) {
                        snprintf(form.error_msg, sizeof(form.error_msg),
                                 "Too many projects (max %d).", MAX_PROJECTS);
                    } else {
                        Project *np = &state.projects[state.project_count];
                        memset(np, 0, sizeof(Project));
                        strncpy(np->name, form.new_name, MAX_NAME - 1);
                        np->name[MAX_NAME - 1] = '\0';
                        state.project_count++;
                        save_config(&state);
                        memset(&form, 0, sizeof(FormState));
                        form.active_field = FORM_FIELD_NAME;
                        restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                        view_mode = MODE_LIST;
                    }
                } else {
                    // add product: advance to path on first Enter
                    if (form.active_field == FORM_FIELD_NAME) {
                        if (form.new_name[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg), "Name cannot be empty.");
                        } else {
                            form.active_field = FORM_FIELD_PATH;
                            form.error_msg[0] = '\0';
                        }
                    } else {
                        if (form.new_name[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg), "Name cannot be empty.");
                            form.active_field = FORM_FIELD_NAME;
                        } else if (form.new_path[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg), "Path cannot be empty.");
                        } else {
                            if (add_product_to_state(&state, form.target_project_index,
                                                     form.new_name, form.new_path) == 0) {
                                save_config(&state);
                                state.selected_index = get_total_product_count(&state) - 1;
                                if (state.selected_index < 0) state.selected_index = 0;
                                memset(&form, 0, sizeof(FormState));
                                form.active_field = FORM_FIELD_NAME;
                                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                                view_mode = MODE_LIST;
                            } else {
                                snprintf(form.error_msg, sizeof(form.error_msg),
                                         "Project full (max %d products).", MAX_PRODUCTS);
                            }
                        }
                    }
                }

            } else {
                if (form.active_field == FORM_FIELD_NAME)
                    buf_append(form.new_name, sizeof(form.new_name), key);
                else if (form.form_mode == FORM_MODE_ADD_PRODUCT)
                    buf_append(form.new_path, sizeof(form.new_path), key);
                form.error_msg[0] = '\0';
            }
        }
    }

    notcurses_stop(nc);
    return 0;
}
