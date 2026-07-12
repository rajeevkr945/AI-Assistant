#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_interface(struct notcurses* nc, struct ncplane* left, struct ncplane* right, AppState* state) {
    (void)nc; // Unused for now
    
    // Clear the planes before drawing the new frame
    ncplane_erase(left);
    ncplane_erase(right);

    // Get dimensions
    unsigned int l_rows, l_cols;
    ncplane_dim_yx(left, &l_rows, &l_cols);
    unsigned int r_rows, r_cols;
    ncplane_dim_yx(right, &r_rows, &r_cols);

    // --- Left Plane: Workspace Products ---
    ncplane_set_fg_rgb(left, 0x0088FF); // Cool Blue Header
    ncplane_on_styles(left, NCSTYLE_BOLD);
    ncplane_putstr_yx(left, 1, 1, "── WORKSPACE PRODUCTS ──");
    ncplane_off_styles(left, NCSTYLE_BOLD);

    int total_products = get_total_product_count(state);
    if (total_products == 0) {
        ncplane_set_fg_rgb(left, 0x888888);
        ncplane_putstr_yx(left, 3, 2, "No products found in ~/.subash.conf");
    } else {
        int current_idx = 0;
        for (int i = 0; i < state->project_count; i++) {
            const Project *proj = &state->projects[i];
            for (int j = 0; j < proj->product_count; j++) {
                const Product *prod = &proj->products[j];
                int y_pos = 3 + current_idx;
                
                // Prevent rendering beyond the plane's visible rows
                if (y_pos >= (int)l_rows) {
                    break;
                }

                if (current_idx == state->selected_index) {
                    // Highlight the currently selected product
                    ncplane_set_fg_rgb(left, 0x00FF00); // Green selection cursor
                    ncplane_on_styles(left, NCSTYLE_BOLD);
                    ncplane_printf_yx(left, y_pos, 1, " > %s", prod->name);
                    ncplane_off_styles(left, NCSTYLE_BOLD);
                    
                    // Show parent project name next to it in muted colors
                    ncplane_set_fg_rgb(left, 0x888888);
                    ncplane_printf(left, " (%s)", proj->name);
                } else {
                    ncplane_set_fg_rgb(left, 0xFFFFFF); // White for inactive
                    ncplane_printf_yx(left, y_pos, 1, "   %s", prod->name);
                    
                    ncplane_set_fg_rgb(left, 0x555555);
                    ncplane_printf(left, " (%s)", proj->name);
                }
                current_idx++;
            }
        }
    }

    // --- Right Plane: Metadata Details ---
    ncplane_set_fg_rgb(right, 0x0088FF); // Cool Blue Header
    ncplane_on_styles(right, NCSTYLE_BOLD);
    ncplane_putstr_yx(right, 1, 1, "── PRODUCT METADATA ──");
    ncplane_off_styles(right, NCSTYLE_BOLD);

    const Project *selected_proj = NULL;
    const Product *selected_prod = get_product_at(state, state->selected_index, &selected_proj);

    if (selected_prod) {
        int row = 3;
        
        // Product & Project Name
        ncplane_set_fg_rgb(right, 0xFFFFFF);
        ncplane_on_styles(right, NCSTYLE_BOLD);
        ncplane_printf_yx(right, row++, 1, "Product:     ");
        ncplane_off_styles(right, NCSTYLE_BOLD);
        ncplane_set_fg_rgb(right, 0x00FF00); // Green product name
        ncplane_printf(right, "%s", selected_prod->name);

        ncplane_set_fg_rgb(right, 0xFFFFFF);
        ncplane_on_styles(right, NCSTYLE_BOLD);
        ncplane_printf_yx(right, row++, 1, "Project:     ");
        ncplane_off_styles(right, NCSTYLE_BOLD);
        ncplane_set_fg_rgb(right, 0xFFFF00); // Yellow project name
        ncplane_printf(right, "%s", selected_proj ? selected_proj->name : "N/A");

        // Local Directory Path
        ncplane_set_fg_rgb(right, 0xFFFFFF);
        ncplane_on_styles(right, NCSTYLE_BOLD);
        ncplane_printf_yx(right, row++, 1, "Local Path:  ");
        ncplane_off_styles(right, NCSTYLE_BOLD);
        ncplane_set_fg_rgb(right, 0x00FFFF); // Cyan path
        ncplane_printf(right, "%s", selected_prod->local_path);

        // SSH Endpoint
        ncplane_set_fg_rgb(right, 0xFFFFFF);
        ncplane_on_styles(right, NCSTYLE_BOLD);
        ncplane_printf_yx(right, row++, 1, "SSH Remote:  ");
        ncplane_off_styles(right, NCSTYLE_BOLD);
        if (selected_prod->remote_ssh[0] != '\0') {
            ncplane_set_fg_rgb(right, 0xFF00FF); // Magenta endpoint
            ncplane_printf(right, "%s", selected_prod->remote_ssh);
        } else {
            ncplane_set_fg_rgb(right, 0x888888);
            ncplane_printf(right, "None");
        }

        // Links
        row++;
        ncplane_set_fg_rgb(right, 0xFFFFFF);
        ncplane_on_styles(right, NCSTYLE_BOLD);
        ncplane_printf_yx(right, row++, 1, "Links:");
        ncplane_off_styles(right, NCSTYLE_BOLD);

        if (selected_prod->link_count > 0) {
            for (int k = 0; k < selected_prod->link_count; k++) {
                if (row >= (int)r_rows - 3) {
                    break; // Keep room for help footer
                }
                ncplane_set_fg_rgb(right, 0x0088FF);
                ncplane_printf_yx(right, row++, 3, "• ");
                ncplane_set_fg_rgb(right, 0x00A8FF);
                ncplane_printf(right, "%s", selected_prod->links[k]);
            }
        } else {
            ncplane_set_fg_rgb(right, 0x888888);
            ncplane_printf_yx(right, row++, 3, "No links registered.");
        }
    } else {
        ncplane_set_fg_rgb(right, 0x888888);
        ncplane_putstr_yx(right, 3, 2, "Select a workspace product to view metadata details.");
    }

    // --- Bottom Help Footer ---
    if (r_rows > 1) {
        ncplane_set_fg_rgb(right, 0x888888); // Gray color
        ncplane_putstr_yx(right, r_rows - 2, 1, " [j/k/↑/↓] Navigate | [Enter] Switch Context | [q/Esc] Quit");
    }
}
