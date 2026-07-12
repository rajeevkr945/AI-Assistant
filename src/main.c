#include "models.h"
#include "ipc.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <notcurses/notcurses.h>

int main(int argc, char **argv) {
    AppState state;
    if (load_config(&state) != 0) {
        fprintf(stderr, "Error: Failed to load configuration from ~/.subash.conf\n");
        return 1;
    }

    // Clamp selected_index to a valid range on startup
    int total_prods = get_total_product_count(&state);
    if (total_prods > 0) {
        if (state.selected_index >= total_prods || state.selected_index < 0) {
            state.selected_index = 0;
        }
    } else {
        state.selected_index = 0;
    }

    if (argc > 1) {
        const char *target = argv[1];
        const char *found_path = NULL;
        const char *found_name = NULL;
        bool is_project = false;

        // Try to match product name first
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
            if (found_path) {
                break;
            }
        }

        // If not found, try to match project name and default to its first product path
        if (!found_path) {
            for (int i = 0; i < state.project_count; i++) {
                Project *proj = &state.projects[i];
                if (strcmp(proj->name, target) == 0) {
                    if (proj->product_count > 0) {
                        found_path = proj->products[0].local_path;
                        found_name = proj->name;
                        is_project = true;
                    }
                    break;
                }
            }
        }

        if (found_path) {
            if (trigger_bash_action(found_path) == 0) {
                printf("Jumped to %s '%s' -> %s\n", 
                       is_project ? "Project" : "Product", 
                       found_name, found_path);
                return 0;
            } else {
                fprintf(stderr, "Error: Failed to write IPC trigger.\n");
                return 1;
            }
        } else {
            fprintf(stderr, "Error: No project or product found matching '%s'\n", target);
            return 1;
        }
    }

    // --- TUI Mode (argc == 1) ---
    // Initialize system UTF-8 locale structure
    setlocale(LC_ALL, "");

    struct notcurses_options nopts = {
        .flags = NCOPTION_INHIBIT_SETLOCALE | NCOPTION_SUPPRESS_BANNERS
    };
    struct notcurses* nc = notcurses_init(&nopts, NULL);
    if (!nc) {
        fprintf(stderr, "Error: Failed to initialize notcurses.\n");
        return 1;
    }

    struct ncplane* stdplane = notcurses_stdplane(nc);
    unsigned int rows, cols;
    ncplane_dim_yx(stdplane, &rows, &cols);

    // Compute dimensions for the 40/60 split
    unsigned int left_cols = cols * 40 / 100;
    unsigned int right_cols = cols - left_cols;

    struct ncplane_options left_opts = {
        .y = 0,
        .x = 0,
        .rows = rows,
        .cols = left_cols,
        .userptr = NULL,
        .name = "left_pane",
        .flags = 0,
    };
    struct ncplane* left_pane = ncplane_create(stdplane, &left_opts);

    struct ncplane_options right_opts = {
        .y = 0,
        .x = left_cols,
        .rows = rows,
        .cols = right_cols,
        .userptr = NULL,
        .name = "right_pane",
        .flags = 0,
    };
    struct ncplane* right_pane = ncplane_create(stdplane, &right_opts);

    ncinput ni;
    while (!state.should_quit) {
        // Draw the split screen interface
        draw_interface(nc, left_pane, right_pane, &state);
        notcurses_render(nc);

        // Block for input
        uint32_t key = notcurses_get_blocking(nc, &ni);

        if (key == NCKEY_RESIZE) {
            // Handle terminal resizing
            ncplane_dim_yx(stdplane, &rows, &cols);
            left_cols = cols * 40 / 100;
            right_cols = cols - left_cols;
            
            ncplane_resize(left_pane, 0, 0, 0, 0, 0, 0, rows, left_cols);
            ncplane_resize(right_pane, 0, 0, 0, 0, 0, 0, rows, right_cols);
            ncplane_move_yx(right_pane, 0, left_cols);
            continue;
        }

        int total = get_total_product_count(&state);
        if (key == 'q' || key == 'Q' || key == NCKEY_ESC) {
            state.should_quit = true;
        } else if (key == 'j' || key == 'J' || key == NCKEY_DOWN) {
            if (total > 0) {
                state.selected_index = (state.selected_index + 1) % total;
            }
        } else if (key == 'k' || key == 'K' || key == NCKEY_UP) {
            if (total > 0) {
                if (state.selected_index > 0) {
                    state.selected_index--;
                } else {
                    state.selected_index = total - 1;
                }
            }
        } else if (key == '\n' || key == '\r' || key == NCKEY_ENTER) {
            const Project *selected_proj = NULL;
            Product *selected_prod = get_product_at(&state, state.selected_index, &selected_proj);
            if (selected_prod) {
                if (trigger_bash_action(selected_prod->local_path) == 0) {
                    save_config(&state);
                    state.should_quit = true;
                }
            }
        }
    }

    // Stop notcurses and restore terminal modes
    notcurses_stop(nc);
    return 0;
}
