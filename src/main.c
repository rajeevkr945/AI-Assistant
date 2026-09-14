#include <wchar.h>
#include "models.h"
#include "ipc.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <ctype.h>
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

static int count_git_log_lines(const char *cache) {
    int count = 0;
    const char *p = cache;
    while (*p) {
        const char *end = strchr(p, '\n');
        count++;
        if (!end) break;
        p = end + 1;
    }
    return count;
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
// Form-field metadata (task fields)
// ─────────────────────────────────────────────────────────────────────────────
static const FormField TASK_CYCLE[] = {
    FORM_FIELD_PROJECT, FORM_FIELD_NAME, FORM_FIELD_PATH, FORM_FIELD_OBSIDIAN,
    FORM_FIELD_REDMINE, FORM_FIELD_REMOTE_HOST, FORM_FIELD_REMOTE_USER,
    FORM_FIELD_REMOTE_PASS, FORM_FIELD_GERRIT_COMMIT, FORM_FIELD_NOTES,
};
#define TASK_CYCLE_N (sizeof(TASK_CYCLE)/sizeof(TASK_CYCLE[0]))

// task-edit doesn't include FORM_FIELD_PROJECT
static const FormField TASK_EDIT_CYCLE[] = {
    FORM_FIELD_NAME, FORM_FIELD_PATH, FORM_FIELD_OBSIDIAN,
    FORM_FIELD_REDMINE, FORM_FIELD_REMOTE_HOST, FORM_FIELD_REMOTE_USER,
    FORM_FIELD_REMOTE_PASS, FORM_FIELD_GERRIT_COMMIT, FORM_FIELD_NOTES,
};
#define TASK_EDIT_CYCLE_N (sizeof(TASK_EDIT_CYCLE)/sizeof(TASK_EDIT_CYCLE[0]))

static const FormField PROJ_CYCLE[] = {
    FORM_FIELD_NAME, FORM_FIELD_PROJ_DESC, FORM_FIELD_PROJ_GIT_PATH,
    FORM_FIELD_PROJ_GERRIT_URL, FORM_FIELD_PROJ_REDMINE_URL,
};
#define PROJ_CYCLE_N (sizeof(PROJ_CYCLE)/sizeof(PROJ_CYCLE[0]))

static FormField cycle_next(const FormField *cyc, size_t n, FormField cur, int dir) {
    int idx = 0;
    for (size_t i = 0; i < n; i++) if (cyc[i] == cur) { idx = (int)i; break; }
    idx = (idx + dir + (int)n) % (int)n;
    return cyc[idx];
}

// ── Form buffer accessors ────────────────────────────────────────────────────
static char *form_field_buf(FormState *f) {
    switch (f->active_field) {
        case FORM_FIELD_PROJECT:         return f->new_project;
        case FORM_FIELD_NAME:            return f->new_name;
        case FORM_FIELD_PATH:            return f->new_path;
        case FORM_FIELD_OBSIDIAN:        return f->new_obsidian;
        case FORM_FIELD_REDMINE:         return f->new_redmine;
        case FORM_FIELD_REMOTE_HOST:     return f->new_remote_host;
        case FORM_FIELD_REMOTE_USER:     return f->new_remote_user;
        case FORM_FIELD_REMOTE_PASS:     return f->new_remote_pass;
        case FORM_FIELD_GERRIT_COMMIT:   return f->new_gerrit_commit;
        case FORM_FIELD_NOTES:           return f->new_notes;
        case FORM_FIELD_PROJ_DESC:       return f->new_description;
        case FORM_FIELD_PROJ_GERRIT_URL: return f->new_gerrit_url;
        case FORM_FIELD_PROJ_GIT_PATH:   return f->new_git_path;
        case FORM_FIELD_PROJ_REDMINE_URL:return f->new_redmine_url;
        default:                         return f->new_name;
    }
}
static size_t form_field_max(FormState *f) {
    switch (f->active_field) {
        case FORM_FIELD_PROJECT:         return sizeof(f->new_project);
        case FORM_FIELD_NAME:            return sizeof(f->new_name);
        case FORM_FIELD_PATH:            return sizeof(f->new_path);
        case FORM_FIELD_OBSIDIAN:        return sizeof(f->new_obsidian);
        case FORM_FIELD_REDMINE:         return sizeof(f->new_redmine);
        case FORM_FIELD_REMOTE_HOST:     return sizeof(f->new_remote_host);
        case FORM_FIELD_REMOTE_USER:     return sizeof(f->new_remote_user);
        case FORM_FIELD_REMOTE_PASS:     return sizeof(f->new_remote_pass);
        case FORM_FIELD_GERRIT_COMMIT:   return sizeof(f->new_gerrit_commit);
        case FORM_FIELD_NOTES:           return sizeof(f->new_notes);
        case FORM_FIELD_PROJ_DESC:       return sizeof(f->new_description);
        case FORM_FIELD_PROJ_GERRIT_URL: return sizeof(f->new_gerrit_url);
        case FORM_FIELD_PROJ_GIT_PATH:   return sizeof(f->new_git_path);
        case FORM_FIELD_PROJ_REDMINE_URL:return sizeof(f->new_redmine_url);
        default:                         return sizeof(f->new_name);
    }
}

static char *edit_field_buf(EditState *e) {
    switch (e->active_field) {
        case FORM_FIELD_NAME:            return e->name_buf;
        case FORM_FIELD_PATH:            return e->path_buf;
        case FORM_FIELD_OBSIDIAN:        return e->obsidian_buf;
        case FORM_FIELD_REDMINE:         return e->redmine_buf;
        case FORM_FIELD_REMOTE_HOST:     return e->remote_host_buf;
        case FORM_FIELD_REMOTE_USER:     return e->remote_user_buf;
        case FORM_FIELD_REMOTE_PASS:     return e->remote_pass_buf;
        case FORM_FIELD_GERRIT_COMMIT:   return e->gerrit_commit_buf;
        case FORM_FIELD_NOTES:           return e->notes_buf;
        case FORM_FIELD_PROJ_DESC:       return e->description_buf;
        case FORM_FIELD_PROJ_GERRIT_URL: return e->gerrit_url_buf;
        case FORM_FIELD_PROJ_GIT_PATH:   return e->git_path_buf;
        case FORM_FIELD_PROJ_REDMINE_URL:return e->redmine_url_buf;
        default:                         return e->name_buf;
    }
}
static size_t edit_field_max(EditState *e) {
    switch (e->active_field) {
        case FORM_FIELD_NAME:            return sizeof(e->name_buf);
        case FORM_FIELD_PATH:            return sizeof(e->path_buf);
        case FORM_FIELD_OBSIDIAN:        return sizeof(e->obsidian_buf);
        case FORM_FIELD_REDMINE:         return sizeof(e->redmine_buf);
        case FORM_FIELD_REMOTE_HOST:     return sizeof(e->remote_host_buf);
        case FORM_FIELD_REMOTE_USER:     return sizeof(e->remote_user_buf);
        case FORM_FIELD_REMOTE_PASS:     return sizeof(e->remote_pass_buf);
        case FORM_FIELD_GERRIT_COMMIT:   return sizeof(e->gerrit_commit_buf);
        case FORM_FIELD_NOTES:           return sizeof(e->notes_buf);
        case FORM_FIELD_PROJ_DESC:       return sizeof(e->description_buf);
        case FORM_FIELD_PROJ_GERRIT_URL: return sizeof(e->gerrit_url_buf);
        case FORM_FIELD_PROJ_GIT_PATH:   return sizeof(e->git_path_buf);
        case FORM_FIELD_PROJ_REDMINE_URL:return sizeof(e->redmine_url_buf);
        default:                         return sizeof(e->name_buf);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Pane helpers
// ─────────────────────────────────────────────────────────────────────────────
static void hide_panes(struct ncplane* left, struct ncplane* right, unsigned int rows) {
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
        const char *found_path = NULL, *found_name = NULL;
        bool is_project = false;

        if (argc > 2) {
            const char *proj_name = argv[1];
            const char *task_name = argv[2];
            for (int i = 0; i < state.project_count; i++) {
                if (strcmp(state.projects[i].name, proj_name) == 0) {
                    Project *proj = &state.projects[i];
                    for (int j = 0; j < proj->product_count; j++) {
                        if (strcmp(proj->products[j].name, task_name) == 0) {
                            found_path = proj->products[j].local_path;
                            found_name = proj->products[j].name;
                            break;
                        }
                    }
                    break;
                }
            }
        } else {
            const char *target = argv[1];
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
        }
        if (found_path) {
            char expanded[MAX_PATH];
            expand_home(found_path, expanded, sizeof(expanded));
            if (trigger_bash_action(expanded) == 0) {
                printf("Jumped to %s '%s' -> %s\n",
                       is_project ? "Project" : "Task", found_name, expanded);
                return 0;
            }
            fprintf(stderr, "Error: Failed to write IPC trigger.\n");
            return 1;
        }
        if (argc > 2) {
            fprintf(stderr, "Error: No task '%s' matching inside project '%s'\n", argv[2], argv[1]);
        } else {
            fprintf(stderr, "Error: No project or task matching '%s'\n", argv[1]);
        }
        return 1;
    }

    // ── TUI mode ──────────────────────────────────────────────────────────────
    setlocale(LC_ALL, "");

    struct notcurses_options nopts = {
        .flags = NCOPTION_INHIBIT_SETLOCALE | NCOPTION_SUPPRESS_BANNERS
    };
    struct notcurses *nc = notcurses_init(&nopts, NULL);
    if (!nc) { fprintf(stderr, "Error: Failed to init notcurses.\n"); return 1; }

    // Enable mouse (best-effort — ignore failure)
    notcurses_mice_enable(nc, NCMICE_BUTTON_EVENT);

    struct ncplane *stdplane = notcurses_stdplane(nc);
    unsigned int rows, cols;
    ncplane_dim_yx(stdplane, &rows, &cols);

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
    ViewMode prev_mode_before_help = MODE_LIST;
    AnimState anim = { .frame_tick = 0, .intro_frame = 0 };

    FormState form; memset(&form, 0, sizeof(FormState));
    form.active_field = FORM_FIELD_PROJECT;

    ConfirmState confirm; memset(&confirm, 0, sizeof(ConfirmState));
    EditState edit; memset(&edit, 0, sizeof(EditState));
    HelpState help; memset(&help, 0, sizeof(HelpState));
    FilterState filter; memset(&filter, 0, sizeof(FilterState));
    HitMap hit; memset(&hit, 0, sizeof(HitMap));

    // Expand left_pane to full screen for intro
    ncplane_resize_simple(left_pane, rows, cols);
    ncplane_move_yx(left_pane, 0, 0);
    ncplane_resize_simple(right_pane, 1, 1);
    ncplane_move_yx(right_pane, (int)rows, 0);

    uint64_t last_anim_ms  = now_ms();
    uint64_t last_intro_ms = now_ms();

    // Preload git graph for initially selected project
    refresh_git_graph(&state);
    int prev_selected_project = state.selected_project;

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

        // Refresh git graph if project changed
        if (state.selected_project != prev_selected_project) {
            refresh_git_graph(&state);
            prev_selected_project = state.selected_project;
        }

        switch (view_mode) {
            case MODE_INTRO:
                draw_intro(nc, left_pane, right_pane, rows, cols, anim.intro_frame);
                break;
            case MODE_LIST:
                draw_interface(nc, left_pane, right_pane, &state, anim.frame_tick, &filter, &hit);
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
            case MODE_HELP:
                draw_help(nc, stdplane, &help);
                break;
            case MODE_FILTER:
                draw_interface(nc, left_pane, right_pane, &state, anim.frame_tick, &filter, &hit);
                draw_filter_overlay(nc, stdplane, &filter);
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
            if (view_mode == MODE_LIST || view_mode == MODE_FILTER) {
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
            } else if (view_mode == MODE_INTRO) {
                ncplane_resize_simple(left_pane, rows, cols);
                ncplane_move_yx(left_pane, 0, 0);
            }
            continue;
        }

        if (view_mode == MODE_INTRO) continue;

        // ── Global: help toggle from anywhere except forms ────────────────────
        if (view_mode == MODE_LIST || view_mode == MODE_FILTER) {
            if (key == '?' || key == NCKEY_F01) {
                prev_mode_before_help = view_mode;
                view_mode = MODE_HELP;
                help.scroll_offset = 0;
                continue;
            }
        }

        // Mouse click (only relevant on the list view)
        if (view_mode == MODE_LIST &&
            (ni.evtype == NCTYPE_PRESS || ni.evtype == NCTYPE_UNKNOWN) &&
            (key == NCKEY_BUTTON1)) {
            // Translate to right_pane local coords: right pane origin = (2, left_cols)
            int rel_y = (int)ni.y - 2;
            int rel_x = (int)ni.x - (int)left_cols;
            if (hit.obsidian_target[0] &&
                rel_y == hit.obsidian_y &&
                rel_x >= hit.obsidian_x &&
                rel_x <  hit.obsidian_x + hit.obsidian_w) {
                char expanded[MAX_PATH];
                expand_home(hit.obsidian_target, expanded, sizeof(expanded));
                if (trigger_open_action(expanded) == 0) {
                    state.should_quit = true;
                }
            }
            continue;
        }

        // ============= MODE_LIST =============
        if (view_mode == MODE_LIST) {
            if (key == 'q' || key == 'Q' || key == NCKEY_ESC) {
                state.should_quit = true;

            } else if (key == NCKEY_LEFT || key == 'h' || key == 'H') {
                if (state.nav_focus == NAV_FOCUS_GIT_LOG) {
                    state.nav_focus = NAV_FOCUS_PRODUCT;
                } else {
                    state.nav_focus = NAV_FOCUS_PROJECT;
                }

            } else if (key == NCKEY_RIGHT || key == 'l' || key == 'L') {
                if (state.nav_focus == NAV_FOCUS_PROJECT) {
                    state.nav_focus = NAV_FOCUS_PRODUCT;
                    normalize_selection(&state);
                } else if (state.nav_focus == NAV_FOCUS_PRODUCT) {
                    if (state.selected_project >= 0 && state.selected_project < state.project_count) {
                        const Project *proj = &state.projects[state.selected_project];
                        if (proj->local_git_path[0]) {
                            state.nav_focus = NAV_FOCUS_GIT_LOG;
                        }
                    }
                }

            } else if (key == 'j' || key == 'J' || key == NCKEY_DOWN) {
                if (state.nav_focus == NAV_FOCUS_PROJECT) {
                    if (state.project_count > 0) {
                        state.selected_project = (state.selected_project + 1) % state.project_count;
                        state.selected_product = 0;
                        normalize_selection(&state);
                    }
                } else if (state.nav_focus == NAV_FOCUS_PRODUCT) {
                    if (state.project_count > 0) {
                        Project *p = &state.projects[state.selected_project];
                        if (p->product_count > 0)
                            state.selected_product = (state.selected_product + 1) % p->product_count;
                    }
                } else if (state.nav_focus == NAV_FOCUS_GIT_LOG) {
                    int total_lines = count_git_log_lines(state.git_graph_cache);
                    int graph_h = 12;
                    if ((int)rows < 6 + graph_h + 6) graph_h = 0;
                    int gline_max = graph_h - 3;
                    if (gline_max < 0) gline_max = 0;
                    int max_scroll = total_lines - gline_max;
                    if (max_scroll < 0) max_scroll = 0;
                    if (state.git_log_scroll < max_scroll) {
                        state.git_log_scroll++;
                    }
                }

            } else if (key == 'k' || key == 'K' || key == NCKEY_UP) {
                if (state.nav_focus == NAV_FOCUS_PROJECT) {
                    if (state.project_count > 0) {
                        state.selected_project = state.selected_project > 0
                            ? state.selected_project - 1
                            : state.project_count - 1;
                        state.selected_product = 0;
                        normalize_selection(&state);
                    }
                } else if (state.nav_focus == NAV_FOCUS_PRODUCT) {
                    if (state.project_count > 0) {
                        Project *p = &state.projects[state.selected_project];
                        if (p->product_count > 0)
                            state.selected_product = state.selected_product > 0
                                ? state.selected_product - 1
                                : p->product_count - 1;
                    }
                } else if (state.nav_focus == NAV_FOCUS_GIT_LOG) {
                    if (state.git_log_scroll > 0) {
                        state.git_log_scroll--;
                    }
                }

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                Product *prod = get_selected_product(&state, NULL);
                if (prod && prod->local_path[0]) {
                    char expanded[MAX_PATH];
                    expand_home(prod->local_path, expanded, sizeof(expanded));
                    if (trigger_bash_action(expanded) == 0) {
                        save_config(&state);
                        state.should_quit = true;
                    }
                }

            // Add task
            } else if (key == 'a' || key == 'A') {
                memset(&form, 0, sizeof(FormState));
                form.form_mode    = FORM_MODE_ADD_PRODUCT;
                form.active_field = FORM_FIELD_PROJECT;
                form.target_project_index = state.selected_project;
                if (state.selected_project >= 0 && state.selected_project < state.project_count)
                    strncpy(form.new_project, state.projects[state.selected_project].name, MAX_NAME - 1);
                hide_panes(left_pane, right_pane, rows);
                view_mode = MODE_FORM;

            // New project
            } else if (key == 'n' || key == 'N') {
                memset(&form, 0, sizeof(FormState));
                form.active_field = FORM_FIELD_NAME;
                form.form_mode    = FORM_MODE_ADD_PROJECT;
                hide_panes(left_pane, right_pane, rows);
                view_mode = MODE_FORM;

            // Edit task
            } else if (key == 'e' || key == 'E') {
                Product *eprod = get_selected_product(&state, NULL);
                if (eprod) {
                    memset(&edit, 0, sizeof(EditState));
                    edit.target        = EDIT_TARGET_PRODUCT;
                    edit.project_index = state.selected_project;
                    edit.product_index = state.selected_product;
                    edit.active_field  = FORM_FIELD_NAME;
                    strncpy(edit.name_buf,          eprod->name,          MAX_NAME - 1);
                    strncpy(edit.path_buf,          eprod->local_path,    MAX_PATH - 1);
                    strncpy(edit.obsidian_buf,      eprod->obsidian_path, MAX_PATH - 1);
                    strncpy(edit.redmine_buf,       eprod->redmine,       MAX_LINK_LEN - 1);
                    strncpy(edit.remote_host_buf,   eprod->remote_host,   MAX_PATH - 1);
                    strncpy(edit.remote_user_buf,   eprod->remote_user,   MAX_NAME - 1);
                    strncpy(edit.remote_pass_buf,   eprod->remote_pass,   MAX_NAME - 1);
                    strncpy(edit.gerrit_commit_buf, eprod->gerrit_commit, MAX_LINK_LEN - 1);
                    strncpy(edit.notes_buf,         eprod->notes,         MAX_NOTES - 1);
                    hide_panes(left_pane, right_pane, rows);
                    view_mode = MODE_EDIT;
                }

            // Edit project details
            } else if (key == 'P') {
                if (state.selected_project >= 0 && state.selected_project < state.project_count) {
                    Project *ep = &state.projects[state.selected_project];
                    memset(&edit, 0, sizeof(EditState));
                    edit.target        = EDIT_TARGET_PROJECT;
                    edit.project_index = state.selected_project;
                    edit.active_field  = FORM_FIELD_NAME;
                    strncpy(edit.name_buf,        ep->name,           MAX_NAME - 1);
                    strncpy(edit.description_buf, ep->description,    MAX_DESC - 1);
                    strncpy(edit.gerrit_url_buf,  ep->gerrit_url,     MAX_LINK_LEN - 1);
                    strncpy(edit.git_path_buf,    ep->local_git_path, MAX_PATH - 1);
                    strncpy(edit.redmine_url_buf, ep->redmine_url,    MAX_LINK_LEN - 1);
                    hide_panes(left_pane, right_pane, rows);
                    view_mode = MODE_EDIT;
                }

            // Delete task
            } else if (key == 'd') {
                Product *dprod = get_selected_product(&state, NULL);
                if (dprod) {
                    memset(&confirm, 0, sizeof(ConfirmState));
                    confirm.action        = CONFIRM_DELETE_PRODUCT;
                    confirm.project_index = state.selected_project;
                    confirm.product_index = state.selected_product;
                    strncpy(confirm.subject_name, dprod->name, MAX_NAME - 1);
                    hide_panes(left_pane, right_pane, rows);
                    view_mode = MODE_CONFIRM;
                }

            // Delete project
            } else if (key == 'D') {
                if (state.selected_project >= 0 && state.selected_project < state.project_count) {
                    Project *dp = &state.projects[state.selected_project];
                    memset(&confirm, 0, sizeof(ConfirmState));
                    confirm.action        = CONFIRM_DELETE_PROJECT;
                    confirm.project_index = state.selected_project;
                    strncpy(confirm.subject_name, dp->name, MAX_NAME - 1);
                    hide_panes(left_pane, right_pane, rows);
                    view_mode = MODE_CONFIRM;
                }

            // Open task's Obsidian doc
            } else if (key == 'o') {
                Product *sp = get_selected_product(&state, NULL);
                if (sp && sp->obsidian_path[0]) {
                    char expanded[MAX_PATH];
                    expand_home(sp->obsidian_path, expanded, sizeof(expanded));
                    if (trigger_open_action(expanded) == 0) state.should_quit = true;
                }

            // Open Gerrit commit URL
            } else if (key == 'g') {
                Product *sp = get_selected_product(&state, NULL);
                if (sp && sp->gerrit_commit[0]) {
                    if (trigger_open_action(sp->gerrit_commit) == 0) state.should_quit = true;
                } else if (state.selected_project >= 0 && state.selected_project < state.project_count) {
                    const char *g = state.projects[state.selected_project].gerrit_url;
                    if (g[0] && trigger_open_action(g) == 0) state.should_quit = true;
                }

            // Open Redmine URL (task-level first, then project-level)
            } else if (key == 'r') {
                Product *sp = get_selected_product(&state, NULL);
                if (sp && sp->redmine[0]) {
                    if (trigger_open_action(sp->redmine) == 0) state.should_quit = true;
                } else if (state.selected_project >= 0 && state.selected_project < state.project_count) {
                    const char *r = state.projects[state.selected_project].redmine_url;
                    if (r[0] && trigger_open_action(r) == 0) state.should_quit = true;
                }

            // Copy path
            } else if (key == 'c') {
                Product *sp = get_selected_product(&state, NULL);
                if (sp && sp->local_path[0]) {
                    char expanded[MAX_PATH];
                    expand_home(sp->local_path, expanded, sizeof(expanded));
                    if (trigger_copy_action(expanded) == 0) state.should_quit = true;
                }

            // Filter
            } else if (key == '/') {
                memset(&filter, 0, sizeof(FilterState));
                view_mode = MODE_FILTER;
            }

        // ============= MODE_FILTER =============
        } else if (view_mode == MODE_FILTER) {
            if (key == NCKEY_ESC) {
                filter.query[0] = '\0';
                view_mode = MODE_LIST;
            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                // Jump to first matching task in current project
                if (state.selected_project >= 0 && state.selected_project < state.project_count) {
                    Project *p = &state.projects[state.selected_project];
                    for (int j = 0; j < p->product_count; j++) {
                        if (fuzzy_match(filter.query, p->products[j].name) ||
                            fuzzy_match(filter.query, p->products[j].local_path)) {
                            state.selected_product = j;
                            state.nav_focus = NAV_FOCUS_PRODUCT;
                            break;
                        }
                    }
                }
                view_mode = MODE_LIST;
            } else if (key == NCKEY_BACKSPACE || key == 127) {
                buf_backspace(filter.query);
            } else {
                buf_append(filter.query, sizeof(filter.query), key);
            }

        // ============= MODE_HELP =============
        } else if (view_mode == MODE_HELP) {
            if (key == NCKEY_ESC || key == '?' || key == 'q' || key == NCKEY_F01) {
                view_mode = prev_mode_before_help;
            } else if (key == 'j' || key == NCKEY_DOWN) {
                help.scroll_offset++;
            } else if (key == 'k' || key == NCKEY_UP) {
                if (help.scroll_offset > 0) help.scroll_offset--;
            }

        // ============= MODE_CONFIRM =============
        } else if (view_mode == MODE_CONFIRM) {
            if (key == 'y' || key == 'Y') {
                if (confirm.action == CONFIRM_DELETE_PRODUCT) {
                    delete_product_in_project(&state, confirm.project_index, confirm.product_index);
                } else {
                    delete_project(&state, confirm.project_index);
                }
                normalize_selection(&state);
                save_config(&state);
                refresh_git_graph(&state);
                prev_selected_project = state.selected_project;
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                view_mode = MODE_LIST;
            } else if (key == 'n' || key == 'N' || key == NCKEY_ESC) {
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                view_mode = MODE_LIST;
            }

        // ============= MODE_EDIT =============
        } else if (view_mode == MODE_EDIT) {
            if (key == NCKEY_ESC) {
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                view_mode = MODE_LIST;

            } else if (key == NCKEY_BACKSPACE || key == 127) {
                buf_backspace(edit_field_buf(&edit));
                edit.error_msg[0] = '\0';

            } else if (key == NCKEY_TAB || key == NCKEY_DOWN) {
                if (edit.target == EDIT_TARGET_PRODUCT)
                    edit.active_field = cycle_next(TASK_EDIT_CYCLE, TASK_EDIT_CYCLE_N, edit.active_field, +1);
                else
                    edit.active_field = cycle_next(PROJ_CYCLE, PROJ_CYCLE_N, edit.active_field, +1);
                edit.error_msg[0] = '\0';

            } else if (key == NCKEY_UP) {
                if (edit.target == EDIT_TARGET_PRODUCT)
                    edit.active_field = cycle_next(TASK_EDIT_CYCLE, TASK_EDIT_CYCLE_N, edit.active_field, -1);
                else
                    edit.active_field = cycle_next(PROJ_CYCLE, PROJ_CYCLE_N, edit.active_field, -1);
                edit.error_msg[0] = '\0';

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                if (edit.target == EDIT_TARGET_PRODUCT) {
                    if (edit.name_buf[0] == '\0') {
                        snprintf(edit.error_msg, sizeof(edit.error_msg), "Name cannot be empty.");
                        edit.active_field = FORM_FIELD_NAME;
                    } else if (edit.project_index >= 0 && edit.project_index < state.project_count) {
                        Project *proj = &state.projects[edit.project_index];
                        if (edit.product_index >= 0 && edit.product_index < proj->product_count) {
                            Product *p = &proj->products[edit.product_index];
                            #define ECOPY(dst, src, sz) do { strncpy((dst),(src),(sz)-1); (dst)[(sz)-1]='\0'; } while(0)
                            ECOPY(p->name,          edit.name_buf,          MAX_NAME);
                            ECOPY(p->local_path,    edit.path_buf,          MAX_PATH);
                            ECOPY(p->obsidian_path, edit.obsidian_buf,      MAX_PATH);
                            ECOPY(p->redmine,       edit.redmine_buf,       MAX_LINK_LEN);
                            ECOPY(p->remote_host,   edit.remote_host_buf,   MAX_PATH);
                            ECOPY(p->remote_user,   edit.remote_user_buf,   MAX_NAME);
                            ECOPY(p->remote_pass,   edit.remote_pass_buf,   MAX_NAME);
                            ECOPY(p->gerrit_commit, edit.gerrit_commit_buf, MAX_LINK_LEN);
                            ECOPY(p->notes,         edit.notes_buf,         MAX_NOTES);
                            #undef ECOPY
                        }
                        save_config(&state);
                        restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                        view_mode = MODE_LIST;
                    }
                } else {
                    if (edit.name_buf[0] == '\0') {
                        snprintf(edit.error_msg, sizeof(edit.error_msg), "Name cannot be empty.");
                    } else {
                        Project *proj = &state.projects[edit.project_index];
                        strncpy(proj->name,           edit.name_buf,        MAX_NAME - 1);
                        strncpy(proj->description,    edit.description_buf, MAX_DESC - 1);
                        strncpy(proj->gerrit_url,     edit.gerrit_url_buf,  MAX_LINK_LEN - 1);
                        strncpy(proj->local_git_path, edit.git_path_buf,    MAX_PATH - 1);
                        strncpy(proj->redmine_url,    edit.redmine_url_buf, MAX_LINK_LEN - 1);
                        save_config(&state);
                        refresh_git_graph(&state);
                        prev_selected_project = state.selected_project;
                        restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                        view_mode = MODE_LIST;
                    }
                }

            } else {
                buf_append(edit_field_buf(&edit), edit_field_max(&edit), key);
                edit.error_msg[0] = '\0';
            }

        // ============= MODE_FORM =============
        } else if (view_mode == MODE_FORM) {
            if (key == NCKEY_ESC) {
                memset(&form, 0, sizeof(FormState));
                form.active_field = FORM_FIELD_PROJECT;
                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                view_mode = MODE_LIST;

            } else if (key == NCKEY_BACKSPACE || key == 127) {
                buf_backspace(form_field_buf(&form));
                form.error_msg[0] = '\0';

            } else if (key == NCKEY_TAB || key == NCKEY_DOWN) {
                if (form.form_mode == FORM_MODE_ADD_PRODUCT)
                    form.active_field = cycle_next(TASK_CYCLE, TASK_CYCLE_N, form.active_field, +1);
                else
                    form.active_field = cycle_next(PROJ_CYCLE, PROJ_CYCLE_N, form.active_field, +1);
                form.error_msg[0] = '\0';

            } else if (key == NCKEY_UP) {
                if (form.form_mode == FORM_MODE_ADD_PRODUCT)
                    form.active_field = cycle_next(TASK_CYCLE, TASK_CYCLE_N, form.active_field, -1);
                else
                    form.active_field = cycle_next(PROJ_CYCLE, PROJ_CYCLE_N, form.active_field, -1);
                form.error_msg[0] = '\0';

            } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
                if (form.form_mode == FORM_MODE_ADD_PROJECT) {
                    if (form.new_name[0] == '\0') {
                        snprintf(form.error_msg, sizeof(form.error_msg), "Name cannot be empty.");
                    } else if (state.project_count >= MAX_PROJECTS) {
                        snprintf(form.error_msg, sizeof(form.error_msg), "Too many projects (max %d).", MAX_PROJECTS);
                    } else if (find_project_by_name(&state, form.new_name) >= 0) {
                        snprintf(form.error_msg, sizeof(form.error_msg), "Project already exists.");
                    } else {
                        Project *np = &state.projects[state.project_count];
                        memset(np, 0, sizeof(Project));
                        strncpy(np->name,           form.new_name,        MAX_NAME - 1);
                        strncpy(np->description,    form.new_description, MAX_DESC - 1);
                        strncpy(np->gerrit_url,     form.new_gerrit_url,  MAX_LINK_LEN - 1);
                        strncpy(np->local_git_path, form.new_git_path,    MAX_PATH - 1);
                        strncpy(np->redmine_url,    form.new_redmine_url, MAX_LINK_LEN - 1);
                        state.project_count++;
                        state.selected_project = state.project_count - 1;
                        state.selected_product = 0;
                        save_config(&state);
                        refresh_git_graph(&state);
                        prev_selected_project = state.selected_project;
                        memset(&form, 0, sizeof(FormState));
                        restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                        view_mode = MODE_LIST;
                    }
                } else {
                    // Task: advance if not last, else save.
                    bool is_last = (form.active_field == FORM_FIELD_NOTES);
                    if (!is_last) {
                        if (form.active_field == FORM_FIELD_PROJECT && form.new_project[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg), "Project name cannot be empty.");
                        } else if (form.active_field == FORM_FIELD_NAME && form.new_name[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg), "Task name cannot be empty.");
                        } else {
                            form.active_field = cycle_next(TASK_CYCLE, TASK_CYCLE_N, form.active_field, +1);
                            form.error_msg[0] = '\0';
                        }
                    } else {
                        if (form.new_project[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg), "Project name cannot be empty.");
                            form.active_field = FORM_FIELD_PROJECT;
                        } else if (form.new_name[0] == '\0') {
                            snprintf(form.error_msg, sizeof(form.error_msg), "Task name cannot be empty.");
                            form.active_field = FORM_FIELD_NAME;
                        } else {
                            int pidx = ensure_project(&state, form.new_project);
                            if (pidx < 0) {
                                snprintf(form.error_msg, sizeof(form.error_msg), "Too many projects (max %d).", MAX_PROJECTS);
                            } else if (add_product_to_state(&state, pidx,
                                        form.new_name, form.new_path,
                                        NULL, NULL, form.new_notes,
                                        form.new_obsidian, form.new_remote_host,
                                        form.new_remote_user, form.new_remote_pass,
                                        form.new_redmine, form.new_gerrit_commit) == 0) {
                                state.selected_project = pidx;
                                state.selected_product = state.projects[pidx].product_count - 1;
                                state.nav_focus = NAV_FOCUS_PRODUCT;
                                save_config(&state);
                                refresh_git_graph(&state);
                                prev_selected_project = state.selected_project;
                                memset(&form, 0, sizeof(FormState));
                                restore_panes(left_pane, right_pane, rows, left_cols, right_cols);
                                view_mode = MODE_LIST;
                            } else {
                                snprintf(form.error_msg, sizeof(form.error_msg), "Project full (max %d tasks).", MAX_PRODUCTS);
                            }
                        }
                    }
                }
            } else {
                buf_append(form_field_buf(&form), form_field_max(&form), key);
                form.error_msg[0] = '\0';
            }
        }
    }

    notcurses_stop(nc);
    return 0;
}
