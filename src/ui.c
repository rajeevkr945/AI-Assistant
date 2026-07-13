#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// Background Matrix Animation (Stopped / Clean Solid Background)
// ─────────────────────────────────────────────────────────────────────────────
void update_background_animation(struct ncplane* plane, uint64_t frame_tick) {
    (void)plane;
    (void)frame_tick;
}

// ─────────────────────────────────────────────────────────────────────────────
// Custom panel border drawing helper
// ─────────────────────────────────────────────────────────────────────────────
static void draw_panel_border(struct ncplane* p, uint32_t color, const char* title) {
    unsigned int rows, cols;
    ncplane_dim_yx(p, &rows, &cols);
    if (rows < 2 || cols < 2) return;

    ncplane_set_fg_rgb(p, color);

    // Top border
    ncplane_putstr_yx(p, 0, 0, "┌");
    for (unsigned int c = 1; c < cols - 1; c++) {
        ncplane_putstr_yx(p, 0, (int)c, "─");
    }
    ncplane_putstr_yx(p, 0, (int)cols - 1, "┐");

    // Sides
    for (unsigned int r = 1; r < rows - 1; r++) {
        ncplane_putstr_yx(p, (int)r, 0, "│");
        ncplane_putstr_yx(p, (int)r, (int)cols - 1, "│");
    }

    // Bottom
    ncplane_putstr_yx(p, (int)rows - 1, 0, "└");
    for (unsigned int c = 1; c < cols - 1; c++) {
        ncplane_putstr_yx(p, (int)rows - 1, (int)c, "─");
    }
    ncplane_putstr_yx(p, (int)rows - 1, (int)cols - 1, "┘");

    // Title label
    if (title && title[0] != '\0') {
        ncplane_set_fg_rgb(p, 0xE3E0F7); // text color
        ncplane_on_styles(p, NCSTYLE_BOLD);
        ncplane_printf_yx(p, 0, 2, " %s ", title);
        ncplane_off_styles(p, NCSTYLE_BOLD);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw Intro
// ─────────────────────────────────────────────────────────────────────────────
void draw_intro(struct notcurses* nc,
                struct ncplane* left, struct ncplane* right,
                unsigned int full_rows, unsigned int full_cols,
                int intro_frame) {
    (void)nc;
    (void)right;

    if (full_rows < 2) full_rows = 2;
    if (full_cols < 8) full_cols = 8;
    if (intro_frame < 0)             intro_frame = 0;
    if (intro_frame >= INTRO_FRAMES) intro_frame = INTRO_FRAMES - 1;

    ncplane_erase(left);

    // Set background to solid dark color
    ncplane_set_bg_rgb(left, 0x121221);
    for (unsigned int r = 0; r < full_rows; r++) {
        for (unsigned int c = 0; c < full_cols; c++) {
            ncplane_putchar_yx(left, (int)r, (int)c, ' ');
        }
    }

    // Centers 7 rows x 40 cols card
    const unsigned int card_h = 7;
    const unsigned int card_w = 40;
    int cy = (int)((full_rows - card_h) / 2);
    int cx = (int)((full_cols - card_w) / 2);
    if (cy < 0) cy = 0;
    if (cx < 0) cx = 0;

    if (intro_frame >= 1) {
        // Draw centered box with Stitch background
        ncplane_set_bg_rgb(left, 0x1E1E2E);
        for (unsigned int r = 0; r < card_h; r++) {
            for (unsigned int c = 0; c < card_w; c++) {
                ncplane_putchar_yx(left, cy + (int)r, cx + (int)c, ' ');
            }
        }

        uint32_t border_color = 0x585B70;
        ncplane_set_fg_rgb(left, border_color);
        
        ncplane_putstr_yx(left, cy, cx, "┌──────────────────────────────────────┐");
        ncplane_putstr_yx(left, cy + 1, cx, "│                                      │");
        ncplane_putstr_yx(left, cy + 5, cx, "└──────────────────────────────────────┘");

        uint32_t title_color = 0x00E478; // green
        ncplane_set_fg_rgb(left, title_color);
        ncplane_on_styles(left, NCSTYLE_BOLD);
        ncplane_putstr_yx(left, cy + 2, cx, "│             S U B A S H              │");
        ncplane_putstr_yx(left, cy + 3, cx, "│      workspace context switcher      │");
        ncplane_putstr_yx(left, cy + 4, cx, "│                                      │");
        ncplane_off_styles(left, NCSTYLE_BOLD);

        if (intro_frame >= INTRO_FRAMES - 2) {
            ncplane_set_fg_rgb(left, 0x00E478);
            ncplane_putstr_yx(left, cy + 4, cx + 5, "[ loading system dashboard... ]");
        }
        ncplane_set_bg_rgb(left, 0x000000);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw Interface (MODE_LIST)
// ─────────────────────────────────────────────────────────────────────────────
void draw_interface(struct notcurses* nc,
                    struct ncplane* left, struct ncplane* right,
                    AppState* state, uint64_t frame_tick) {
    struct ncplane* stdplane = notcurses_stdplane(nc);
    ncplane_erase(stdplane);

    unsigned int rows, cols;
    ncplane_dim_yx(stdplane, &rows, &cols);

    // ── 1. Set full standard plane background to dark blue-purple ──
    ncplane_set_bg_rgb(stdplane, 0x121221);
    for (unsigned int r = 0; r < rows; r++) {
        for (unsigned int c = 0; c < cols; c++) {
            ncplane_putchar_yx(stdplane, (int)r, (int)c, ' ');
        }
    }

    // ── 2. Draw Top Bar on standard plane ──
    // Title
    ncplane_set_fg_rgb(stdplane, 0x00E478); // Stitch green
    ncplane_on_styles(stdplane, NCSTYLE_BOLD);
    ncplane_putstr_yx(stdplane, 0, 2, "⚡ SYSTEM::DASHBOARD");
    ncplane_off_styles(stdplane, NCSTYLE_BOLD);

    // Top border split line
    ncplane_set_fg_rgb(stdplane, 0x585B70);
    for (unsigned int c = 0; c < cols; c++) {
        ncplane_putstr_yx(stdplane, 1, (int)c, "─");
    }

    // ── 3. Draw Footer on standard plane ──
    int footer_y = (int)rows - 1;
    // Bottom border divider
    ncplane_set_fg_rgb(stdplane, 0x585B70);
    for (unsigned int c = 0; c < cols; c++) {
        ncplane_putstr_yx(stdplane, footer_y - 1, (int)c, "─");
    }

    // Hotkey list
    ncplane_set_fg_rgb(stdplane, 0x888888);
    ncplane_putstr_yx(stdplane, footer_y, 2, "󰌌 ");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[a]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Add Item  ");
    ncplane_set_fg_rgb(stdplane, 0x888888);
    ncplane_putstr(stdplane, "[m] Toggle Mode  ");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[j/k]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Navigate  ");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[q]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Quit");

    // Version info
    int version_x = (int)cols - 25;
    if (version_x > 50) {
        ncplane_set_fg_rgb(stdplane, 0x00E478);
        ncplane_putstr_yx(stdplane, footer_y, version_x, "v1.0.4-stable");
        ncplane_set_fg_rgb(stdplane, 0x585B70);
        ncplane_putstr(stdplane, " │ ");
        ncplane_set_fg_rgb(stdplane, 0x888888);
        ncplane_putstr(stdplane, "uptime: 142h");
    }

    // ── 4. Render Left Pane (Sidebar) ──
    ncplane_erase(left);
    ncplane_set_bg_rgb(left, 0x121221);
    
    unsigned int l_rows, l_cols;
    ncplane_dim_yx(left, &l_rows, &l_cols);

    // Sidebar border
    draw_panel_border(left, 0x585B70, "");

    // Section 1: Navigation header
    int nav_y = 1;
    ncplane_set_fg_rgb(left, 0x888888);
    ncplane_putstr_yx(left, nav_y, 1, "┌─ NAVIGATION");
    unsigned int label_len = 13;
    if (l_cols > label_len + 2) {
        ncplane_set_fg_rgb(left, 0x585B70);
        for (unsigned int c = label_len + 1; c < l_cols - 2; c++) {
            ncplane_putstr_yx(left, nav_y, (int)c, "─");
        }
        ncplane_putstr_yx(left, nav_y, (int)l_cols - 2, "┐");
    }

    // Find active project from selected index
    const Project* sel_proj = NULL;
    get_product_at(state, state->selected_index, &sel_proj);

    // List Projects as navigation entries (taking up the rest of the height)
    int row = nav_y + 1;
    for (int i = 0; i < state->project_count; i++) {
        if (row >= (int)l_rows - 2) break;
        const Project* proj = &state->projects[i];
        bool is_active = (sel_proj && strcmp(proj->name, sel_proj->name) == 0);

        if (is_active) {
            ncplane_set_fg_rgb(left, 0x00E478);
            ncplane_putstr_yx(left, row, 2, "❯ ");
            ncplane_set_fg_rgb(left, 0x89B4FA); // blue
            ncplane_on_styles(left, NCSTYLE_BOLD);
            ncplane_printf(left, "%s", proj->name);
            ncplane_off_styles(left, NCSTYLE_BOLD);
        } else {
            ncplane_set_fg_rgb(left, 0x556677);
            ncplane_putstr_yx(left, row, 2, "  ");
            ncplane_printf(left, "%s", proj->name);
        }
        row++;
    }

    // ── 5. Render Right Pane (Workspace / Process Registry) ──
    ncplane_erase(right);
    ncplane_set_bg_rgb(right, 0x121221);
    
    unsigned int r_rows, r_cols;
    ncplane_dim_yx(right, &r_rows, &r_cols);

    // Border
    draw_panel_border(right, 0x585B70, "");

    // Panel Header label (PROCESS REGISTRY)
    ncplane_set_fg_rgb(right, 0xE3E0F7);
    ncplane_on_styles(right, NCSTYLE_BOLD);
    ncplane_putstr_yx(right, 0, 2, "┌─ PROCESS REGISTRY ──────┐");
    ncplane_off_styles(right, NCSTYLE_BOLD);

    int total_prods = get_total_product_count(state);
    ncplane_set_fg_rgb(right, 0x556677);
    ncplane_printf_yx(right, 0, 30, " Listing: %02d total nodes ", total_prods);

    // Table Headers (Optimized column widths to give maximum space to PATH)
    int table_y = 2;
    ncplane_set_fg_rgb(right, 0x888888);
    ncplane_on_styles(right, NCSTYLE_BOLD);
    ncplane_printf_yx(right, table_y, 2, "%-4s   %-15s   %-12s   %-10s   %s", "ID", "COMPONENT NAME", "PROJECT", "STATUS", "PATH");
    ncplane_off_styles(right, NCSTYLE_BOLD);

    // Header divider line
    ncplane_set_fg_rgb(right, 0x585B70);
    ncplane_putstr_yx(right, table_y + 1, 1, "─");
    for (unsigned int c = 2; c < r_cols - 1; c++) {
        ncplane_putstr_yx(right, table_y + 1, (int)c, "─");
    }

    // Render table rows (Now extends full height to bottom stats bar!)
    int py = table_y + 2;
    int table_max_rows = (int)r_rows - 5;

    int flat_idx = 0;
    for (int i = 0; i < state->project_count; i++) {
        const Project* proj = &state->projects[i];
        for (int j = 0; j < proj->product_count; j++) {
            if (py >= table_y + 2 + table_max_rows) break;
            const Product* prod = &proj->products[j];
            bool selected = (flat_idx == state->selected_index);

            if (selected) {
                ncplane_set_fg_rgb(right, 0x00E478);
                ncplane_putstr_yx(right, py, 1, "❯");
                ncplane_on_styles(right, NCSTYLE_BOLD);
            } else {
                ncplane_putstr_yx(right, py, 1, " ");
            }

            // ID
            ncplane_set_fg_rgb(right, 0x888888);
            ncplane_printf_yx(right, py, 3, "%03d", flat_idx + 1);

            // Product Name (starts at 8, width 15)
            if (selected) {
                ncplane_set_fg_rgb(right, 0x89B4FA);
            } else {
                ncplane_set_fg_rgb(right, 0xE3E0F7);
            }
            ncplane_printf_yx(right, py, 8, "%-15s", prod->name);

            // Project Name (starts at 25, width 12)
            ncplane_set_fg_rgb(right, 0x556677);
            ncplane_printf_yx(right, py, 25, "%-12s", proj->name);

            // Status (starts at 39, width 10)
            if (selected) {
                ncplane_set_fg_rgb(right, 0x00E478);
                ncplane_printf_yx(right, py, 39, "[ ACTIVE ]");
            } else {
                if (flat_idx % 4 == 0) {
                    ncplane_set_fg_rgb(right, 0x00E478);
                    ncplane_printf_yx(right, py, 39, "[ ACTIVE ]");
                } else if (flat_idx % 4 == 1) {
                    ncplane_set_fg_rgb(right, 0x89B4FA);
                    ncplane_printf_yx(right, py, 39, "[ NOMINAL]");
                } else if (flat_idx % 4 == 2) {
                    ncplane_set_fg_rgb(right, 0xFFFF00);
                    ncplane_printf_yx(right, py, 39, "[ TUNING ]");
                } else {
                    ncplane_set_fg_rgb(right, 0xFF4444);
                    ncplane_printf_yx(right, py, 39, "[CRITICAL]");
                }
            }

            // Local Path (starts at 51, extended length)
            ncplane_set_fg_rgb(right, 0x556677);
            int path_start = 51;
            int path_w = (int)r_cols - path_start - 2;
            if (path_w > 0) {
                const char* display_path = prod->local_path;
                size_t path_len = strlen(display_path);
                if ((int)path_len > path_w) {
                    display_path = prod->local_path + (path_len - path_w + 2);
                    ncplane_printf_yx(right, py, path_start, "..%s", display_path);
                } else {
                    ncplane_printf_yx(right, py, path_start, "%s", display_path);
                }
            }

            if (selected) {
                ncplane_off_styles(right, NCSTYLE_BOLD);
            }

            py++;
            flat_idx++;
        }
    }

    // Dotted fill rows
    while (py < table_y + 2 + table_max_rows) {
        ncplane_set_fg_rgb(right, 0x333344);
        ncplane_printf_yx(right, py, 3, "%03d", flat_idx + 1);
        ncplane_putstr_yx(right, py, 8, " | ...................................................................................");
        py++;
        flat_idx++;
    }

    // Divider line above stats bar
    ncplane_set_fg_rgb(right, 0x585B70);
    ncplane_putstr_yx(right, (int)r_rows - 3, 1, "─");
    for (unsigned int c = 2; c < r_cols - 1; c++) {
        ncplane_putstr_yx(right, (int)r_rows - 3, (int)c, "─");
    }

    // ── 7. Render MEM stats & Search Box inside the right panel bottom ──
    int stats_y = (int)r_rows - 2;

    ncplane_set_fg_rgb(right, 0x888888);
    ncplane_putstr_yx(right, stats_y, 2, "MEM: ");
    ncplane_set_fg_rgb(right, 0x89B4FA); // blue
    ncplane_putstr(right, "████░░░░░░ 34%   ");

    // Search bar
    int search_x = (int)r_cols - 28;
    if (search_x > 35) {
        ncplane_set_fg_rgb(right, 0x888888);
        ncplane_putstr_yx(right, stats_y, search_x, "SEARCH: ");
        ncplane_set_bg_rgb(right, 0x1E1E2E);
        ncplane_set_fg_rgb(right, 0x00E478);
        ncplane_printf(right, " / ");
        ncplane_set_fg_rgb(right, 0xE3E0F7);
        ncplane_printf(right, "_ ");
        // Blinking search cursor
        if ((frame_tick / 10) % 2 == 0) {
            ncplane_set_bg_rgb(right, 0x00E478);
            ncplane_putstr(right, " ");
        } else {
            ncplane_set_bg_rgb(right, 0x1E1E2E);
            ncplane_putstr(right, " ");
        }
        ncplane_set_bg_rgb(right, 0x121221); // reset background
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw Form (MODE_FORM)
// ─────────────────────────────────────────────────────────────────────────────
void draw_form(struct notcurses* nc, struct ncplane* std_plane,
               const FormState* form, const AppState* state) {
    (void)nc;
    (void)state;
    ncplane_erase(std_plane);

    unsigned int rows, cols;
    ncplane_dim_yx(std_plane, &rows, &cols);

    // Fill background with Stitch color
    ncplane_set_bg_rgb(std_plane, 0x121221);
    for (unsigned int r = 0; r < rows; r++) {
        for (unsigned int c = 0; c < cols; c++) {
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');
        }
    }

    // ── Draw Top Bar ──
    ncplane_set_fg_rgb(std_plane, 0x00E478);
    ncplane_on_styles(std_plane, NCSTYLE_BOLD);
    ncplane_putstr_yx(std_plane, 0, 2, "DASHBOARD");
    ncplane_off_styles(std_plane, NCSTYLE_BOLD);

    // Divider
    ncplane_set_fg_rgb(std_plane, 0x585B70);
    for (unsigned int c = 0; c < cols; c++) {
        ncplane_putstr_yx(std_plane, 1, (int)c, "─");
    }

    // ── Form Panel Size 13 rows x 64 cols ──
    unsigned int box_h = 13;
    unsigned int box_w = 64;
    if (box_w > cols) box_w = cols;
    if (box_h > rows) box_h = rows;

    unsigned int box_y = (rows > box_h) ? (rows - box_h) / 2 : 0;
    unsigned int box_x = (cols > box_w) ? (cols - box_w) / 2 : 0;

    // Draw main panel container
    ncplane_set_bg_rgb(std_plane, 0x1E1E2E);
    for (unsigned int r = box_y; r < box_y + box_h; r++) {
        for (unsigned int c = box_x; c < box_x + box_w; c++) {
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');
        }
    }

    // Panel border
    draw_panel_border(std_plane, 0x585B70, "SYS_PARAMETERS_ENTRY");

    // Info description
    ncplane_set_fg_rgb(std_plane, 0x00E478);
    ncplane_putstr_yx(std_plane, (int)box_y + 2, (int)box_x + 3, "[ INFO ]");
    ncplane_set_fg_rgb(std_plane, 0xE3E0F7);
    ncplane_printf(std_plane, " Add Details");

    // ── FIELD 1: Product Name input box ──
    unsigned int name_y = box_y + 4;
    uint32_t name_border_color = (form->active_field == FORM_FIELD_NAME) ? 0x00E478 : 0x585B70;
    ncplane_set_fg_rgb(std_plane, name_border_color);
    ncplane_putstr_yx(std_plane, (int)name_y, (int)box_x + 3, "┌─ Product Name ──────────────────────────────────┐");
    ncplane_putstr_yx(std_plane, (int)name_y + 1, (int)box_x + 3, "│ [                                              ] │");
    ncplane_putstr_yx(std_plane, (int)name_y + 2, (int)box_x + 3, "└─────────────────────────────────────────────────┘");

    // Value
    ncplane_set_fg_rgb(std_plane, 0x00E478);
    ncplane_putstr_yx(std_plane, (int)name_y + 1, (int)box_x + 7, form->new_name);
    // Cursor in Name field
    if (form->active_field == FORM_FIELD_NAME) {
        ncplane_set_bg_rgb(std_plane, 0x00E478);
        ncplane_putstr_yx(std_plane, (int)name_y + 1, (int)(box_x + 7 + strlen(form->new_name)), " ");
        ncplane_set_bg_rgb(std_plane, 0x1E1E2E);
    }

    // ── FIELD 2: Local Path input box (Allow longer path text display) ──
    unsigned int path_y = box_y + 7;
    uint32_t path_border_color = (form->active_field == FORM_FIELD_PATH) ? 0x00E478 : 0x585B70;
    ncplane_set_fg_rgb(std_plane, path_border_color);
    ncplane_putstr_yx(std_plane, (int)path_y, (int)box_x + 3, "┌─ Local Path ────────────────────────────────────┐");
    ncplane_putstr_yx(std_plane, (int)path_y + 1, (int)box_x + 3, "│ [                                              ] │");
    ncplane_putstr_yx(std_plane, (int)path_y + 2, (int)box_x + 3, "└─────────────────────────────────────────────────┘");

    // Value (scroll if too long for the box viewport)
    ncplane_set_fg_rgb(std_plane, 0x00E478);
    const char* path_display = form->new_path;
    size_t path_len = strlen(form->new_path);
    if (path_len > 46) {
        path_display = form->new_path + (path_len - 46);
    }
    ncplane_putstr_yx(std_plane, (int)path_y + 1, (int)box_x + 7, path_display);
    // Cursor in Path field
    if (form->active_field == FORM_FIELD_PATH) {
        ncplane_set_bg_rgb(std_plane, 0x00E478);
        int cursor_offset = (path_len > 46) ? 46 : (int)path_len;
        ncplane_putstr_yx(std_plane, (int)path_y + 1, (int)(box_x + 7 + cursor_offset), " ");
        ncplane_set_bg_rgb(std_plane, 0x1E1E2E);
    }

    // Action buttons at the bottom of the panel
    ncplane_set_fg_rgb(std_plane, 0x00E478);
    ncplane_putstr_yx(std_plane, (int)box_y + 10, (int)box_x + 3, "┌──────────┐");
    ncplane_putstr_yx(std_plane, (int)box_y + 11, (int)box_x + 3, "│  [SAVE]  │");
    ncplane_putstr_yx(std_plane, (int)box_y + 12, (int)box_x + 3, "└──────────┘");

    ncplane_set_fg_rgb(std_plane, 0x585B70);
    ncplane_putstr_yx(std_plane, (int)box_y + 10, (int)box_x + 49, "┌──────────┐");
    ncplane_putstr_yx(std_plane, (int)box_y + 11, (int)box_x + 49, "│ [CANCEL] │");
    ncplane_putstr_yx(std_plane, (int)box_y + 12, (int)box_x + 49, "└──────────┘");

    // Error messages if present
    if (form->error_msg[0] != '\0') {
        ncplane_set_fg_rgb(std_plane, 0xFF4444);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf_yx(std_plane, (int)box_y + box_h - 2, (int)box_x + 3, "! ERROR: %s", form->error_msg);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
    }

    // ── Footer on rows - 1 ──
    int footer_y = (int)rows - 1;
    ncplane_set_fg_rgb(std_plane, 0x585B70);
    for (unsigned int c = 0; c < cols; c++) {
        ncplane_putstr_yx(std_plane, footer_y - 1, (int)c, "─");
    }

    ncplane_set_fg_rgb(std_plane, 0x888888);
    ncplane_putstr_yx(std_plane, footer_y, 2, " [Tab/↑↓] Switch Field   [Enter] Save Parameter Commit   [Esc] Cancel Override");
}
