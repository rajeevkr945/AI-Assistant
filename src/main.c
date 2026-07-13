// Enable POSIX.1-2008 extensions for clock_gettime() and nanosleep()
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

// Return monotonic time in milliseconds.
static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

// Sleep for the given number of milliseconds (best-effort, non-signal-safe).
static void sleep_ms(long ms) {
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

// ─────────────────────────────────────────────────────────────────────────────
// Input-buffer helpers  (printable ASCII only)
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
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
    AppState state;
    if (load_config(&state) != 0) {
        fprintf(stderr, "Error: Failed to load configuration from ~/.subash.conf\n");
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
        const char *target     = argv[1];
        const char *found_path = NULL;
        const char *found_name = NULL;
        bool is_project        = false;

        for (int i = 0; i < state.project_count; i++) {
            Project *proj = &state.projects[i];
            for (int j = 0; j < proj->product_count; j++) {
                Product *prod = &proj->products[j];
                if (strcmp(prod->name, target) == 0) {
                    found_path = prod->local_path;
                    found_name = prod->name;
                    is_project = false;
                    break;
                }
            }
            if (found_path) break;
        }

        if (!found_path) {
            for (int i = 0; i < state.project_count; i++) {
                Project *proj = &state.projects[i];
                if (strcmp(proj->name, target) == 0 && proj->product_count > 0) {
                    found_path = proj->products[0].local_path;
                    found_name = proj->name;
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
        fprintf(stderr, "Error: No project or product found matching '%s'\n", target);
        return 1;
    }

    // ── TUI Mode ──────────────────────────────────────────────────────────────
    setlocale(LC_ALL, "");

    struct notcurses_options nopts = {
        .flags = NCOPTION_INHIBIT_SETLOCALE | NCOPTION_SUPPRESS_BANNERS
    };
    struct notcurses *nc = notcurses_init(&nopts, NULL);
    if (!nc) {
        fprintf(stderr, "Error: Failed to initialize notcurses.\n");
        return 1;
    }

    struct ncplane *stdplane = notcurses_stdplane(nc);
    unsigned int rows, cols;
    ncplane_dim_yx(stdplane, &rows, &cols);

    unsigned int left_cols  = cols * 28 / 100;
    unsigned int right_cols = cols - left_cols;

    struct ncplane_options left_opts = {
        .y = 2, .x = 0,
        .rows = rows - 3, .cols = left_cols,
        .userptr = NULL, .name = "left_pane", .flags = 0,
    };
    struct ncplane *left_pane = ncplane_create(stdplane, &left_opts);

    struct ncplane_options right_opts = {
        .y = 2, .x = (int)left_cols,
        .rows = rows - 3, .cols = right_cols,
        .userptr = NULL, .name = "right_pane", .flags = 0,
    };
    struct ncplane *right_pane = ncplane_create(stdplane, &right_opts);

    // ── Application state ─────────────────────────────────────────────────────
    ViewMode view_mode = MODE_INTRO;

    AnimState anim;
    anim.frame_tick  = 0;
    anim.intro_frame = 0;

    FormState form;
    memset(&form, 0, sizeof(FormState));
    form.active_field = FORM_FIELD_NAME;

    // For MODE_INTRO, expand left_pane to the full viewport so it acts as a
    // full-screen canvas. It will be shrunk back to 40% when intro ends.
    ncplane_resize_simple(left_pane, rows, cols);
    ncplane_move_yx(left_pane, 0, 0);
    // Move right_pane off-screen during intro so it doesn't interfere
    ncplane_resize_simple(right_pane, 1, 1);
    ncplane_move_yx(right_pane, (int)rows, 0);

    // Timestamps for the two independent time-based events:
    //   last_anim_ms  – next background animation tick (every ANIM_TICK_MS)
    //   last_intro_ms – next intro frame advance (every INTRO_FRAME_MS)
    uint64_t last_anim_ms  = now_ms();
    uint64_t last_intro_ms = now_ms();

    ncinput ni;

    while (!state.should_quit) {
        uint64_t current_ms = now_ms();

        // ── TIME-BASED EVENTS ─────────────────────────────────────────────────

        // 1. Intro animation: advance one scale frame every INTRO_FRAME_MS ms
        if (view_mode == MODE_INTRO) {
            if (current_ms - last_intro_ms >= INTRO_FRAME_MS) {
                last_intro_ms = current_ms;
                anim.intro_frame++;
                if (anim.intro_frame >= INTRO_FRAMES) {
                    // Restore split panes to their proper 28/72 dimensions
                    ncplane_resize_simple(left_pane,  rows - 3, left_cols);
                    ncplane_move_yx(left_pane,  2, 0);
                    ncplane_resize_simple(right_pane, rows - 3, right_cols);
                    ncplane_move_yx(right_pane, 2, (int)left_cols);
                    view_mode = MODE_LIST;
                }
            }
        }

        // 2. Background animation tick: every ANIM_TICK_MS ms (all modes)
        if (current_ms - last_anim_ms >= ANIM_TICK_MS) {
            last_anim_ms = current_ms;
            anim.frame_tick++;
        }

        // ── RENDER ───────────────────────────────────────────────────────────
        switch (view_mode) {
            case MODE_INTRO:
                draw_intro(nc, left_pane, right_pane,
                           rows, cols, anim.intro_frame);
                break;
            case MODE_LIST:
                draw_interface(nc, left_pane, right_pane, &state, anim.frame_tick);
                break;
            case MODE_FORM:
                draw_form(nc, stdplane, &form, &state);
                break;
        }
        notcurses_render(nc);

        // ── NON-BLOCKING INPUT ────────────────────────────────────────────────
        // Poll for a key; returns 0 immediately if no key is waiting.
        uint32_t key = notcurses_get_nblock(nc, &ni);

        if (key == (uint32_t)-1) {
            // Error — bail out gracefully
            break;
        }

        if (key == 0) {
            // No key pressed — sleep briefly to yield CPU (~5 ms resolution)
            sleep_ms(5);
            continue;
        }

        // ── Terminal resize (all modes) ───────────────────────────────────────
        if (key == NCKEY_RESIZE) {
            ncplane_dim_yx(stdplane, &rows, &cols);
            left_cols  = cols * 28 / 100;
            right_cols = cols - left_cols;
            if (view_mode == MODE_LIST) {
                ncplane_resize_simple(left_pane,  rows - 3, left_cols);
                ncplane_move_yx(left_pane,  2, 0);
                ncplane_resize_simple(right_pane, rows - 3, right_cols);
                ncplane_move_yx(right_pane, 2, (int)left_cols);
            } else if (view_mode == MODE_FORM || view_mode == MODE_INTRO) {
                if (view_mode == MODE_INTRO) {
                    ncplane_resize_simple(left_pane, rows, cols);
                    ncplane_move_yx(left_pane, 0, 0);
                } else {
                    ncplane_resize_simple(left_pane, 1, 1);
                    ncplane_move_yx(left_pane, (int)rows, 0);
                    ncplane_resize_simple(right_pane, 1, 1);
                    ncplane_move_yx(right_pane, (int)rows, 0);
                }
            }
            continue;
        }

        // Swallow all input during intro — user just has to wait 150 ms
        if (view_mode == MODE_INTRO) continue;

        // ================================================================
        // MODE_LIST input
        // ================================================================
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
                                           ? state.selected_index - 1
                                           : total - 1;

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                const Project *selected_proj = NULL;
                Product *selected_prod =
                    get_product_at(&state, state.selected_index, &selected_proj);
                if (selected_prod && trigger_bash_action(selected_prod->local_path) == 0) {
                    save_config(&state);
                    state.should_quit = true;
                }

            } else if (key == 'a' || key == 'A') {
                if (state.project_count > 0) {
                    memset(&form, 0, sizeof(FormState));
                    form.active_field = FORM_FIELD_NAME;
                    // Move panes off-screen during form entry
                    ncplane_resize_simple(left_pane, 1, 1);
                    ncplane_move_yx(left_pane, (int)rows, 0);
                    ncplane_resize_simple(right_pane, 1, 1);
                    ncplane_move_yx(right_pane, (int)rows, 0);
                    view_mode = MODE_FORM;
                }
            }

        // ================================================================
        // MODE_FORM input
        // ================================================================
        } else if (view_mode == MODE_FORM) {
            if (key == NCKEY_ESC) {
                memset(&form, 0, sizeof(FormState));
                form.active_field = FORM_FIELD_NAME;
                // Restore panes to their proper split dimensions
                ncplane_resize_simple(left_pane,  rows - 3, left_cols);
                ncplane_move_yx(left_pane,  2, 0);
                ncplane_resize_simple(right_pane, rows - 3, right_cols);
                ncplane_move_yx(right_pane, 2, (int)left_cols);
                view_mode = MODE_LIST;

            } else if (key == NCKEY_TAB || key == NCKEY_DOWN || key == NCKEY_UP) {
                form.active_field = (form.active_field == FORM_FIELD_NAME)
                                    ? FORM_FIELD_PATH : FORM_FIELD_NAME;
                form.error_msg[0] = '\0';

            } else if (key == NCKEY_BACKSPACE || key == 127) {
                if (form.active_field == FORM_FIELD_NAME)
                    buf_backspace(form.new_name);
                else
                    buf_backspace(form.new_path);
                form.error_msg[0] = '\0';

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                if (form.active_field == FORM_FIELD_NAME) {
                    form.active_field = FORM_FIELD_PATH;
                    form.error_msg[0] = '\0';
                } else {
                    if (form.new_name[0] == '\0') {
                        snprintf(form.error_msg, sizeof(form.error_msg),
                                 "Product name cannot be empty.");
                        form.active_field = FORM_FIELD_NAME;
                    } else if (form.new_path[0] == '\0') {
                        snprintf(form.error_msg, sizeof(form.error_msg),
                                 "Local path cannot be empty.");
                    } else {
                        if (add_product_to_state(&state, 0,
                                                 form.new_name, form.new_path) == 0) {
                            save_config(&state);
                            state.selected_index = get_total_product_count(&state) - 1;
                            if (state.selected_index < 0) state.selected_index = 0;
                            memset(&form, 0, sizeof(FormState));
                            form.active_field = FORM_FIELD_NAME;
                            // Restore panes to their proper split dimensions
                            ncplane_resize_simple(left_pane,  rows - 3, left_cols);
                            ncplane_move_yx(left_pane,  2, 0);
                            ncplane_resize_simple(right_pane, rows - 3, right_cols);
                            ncplane_move_yx(right_pane, 2, (int)left_cols);
                            view_mode = MODE_LIST;
                        } else {
                            snprintf(form.error_msg, sizeof(form.error_msg),
                                     "Project is full (max %d products).", MAX_PRODUCTS);
                        }
                    }
                }

            } else {
                if (form.active_field == FORM_FIELD_NAME)
                    buf_append(form.new_name, sizeof(form.new_name), key);
                else
                    buf_append(form.new_path, sizeof(form.new_path), key);
                form.error_msg[0] = '\0';
            }
        }
    }

    notcurses_stop(nc);
    return 0;
}
