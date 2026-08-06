#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// Background animation (stub — keep clean solid bg)
// ─────────────────────────────────────────────────────────────────────────────
void update_background_animation(struct ncplane* plane, uint64_t frame_tick) {
    (void)plane; (void)frame_tick;
}

// ─────────────────────────────────────────────────────────────────────────────
// Panel border helper
// ─────────────────────────────────────────────────────────────────────────────
static void draw_panel_border(struct ncplane* p, uint32_t color, const char* title) {
    unsigned int rows, cols;
    ncplane_dim_yx(p, &rows, &cols);
    if (rows < 2 || cols < 2) return;

    ncplane_set_fg_rgb(p, color);
    ncplane_putstr_yx(p, 0, 0, "┌");
    for (unsigned int c = 1; c < cols - 1; c++)
        ncplane_putstr_yx(p, 0, (int)c, "─");
    ncplane_putstr_yx(p, 0, (int)cols - 1, "┐");
    for (unsigned int r = 1; r < rows - 1; r++) {
        ncplane_putstr_yx(p, (int)r, 0, "│");
        ncplane_putstr_yx(p, (int)r, (int)cols - 1, "│");
    }
    ncplane_putstr_yx(p, (int)rows - 1, 0, "└");
    for (unsigned int c = 1; c < cols - 1; c++)
        ncplane_putstr_yx(p, (int)rows - 1, (int)c, "─");
    ncplane_putstr_yx(p, (int)rows - 1, (int)cols - 1, "┘");

    if (title && title[0] != '\0') {
        ncplane_set_fg_rgb(p, 0xE3E0F7);
        ncplane_on_styles(p, NCSTYLE_BOLD);
        ncplane_printf_yx(p, 0, 2, " %s ", title);
        ncplane_off_styles(p, NCSTYLE_BOLD);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input field helper (scrolling text + cursor)
// ─────────────────────────────────────────────────────────────────────────────
static void draw_input_field(struct ncplane* p,
                              const char* label,
                              const char* buf,
                              int field_w,
                              bool active,
                              int bx, int ry) {
    uint32_t bcol = active ? 0x00E478 : 0x585B70;
    ncplane_set_fg_rgb(p, bcol);

    ncplane_putstr_yx(p, ry, bx + 3, "┌─ ");
    ncplane_putstr(p, label);
    int dashes = field_w - (int)strlen(label) - 1;
    if (dashes < 0) dashes = 0;
    for (int d = 0; d < dashes; d++) ncplane_putstr(p, "─");
    ncplane_putstr(p, "┐");

    ncplane_putstr_yx(p, ry + 1, bx + 3, "│ [");
    for (int d = 0; d < field_w; d++) ncplane_putstr(p, " ");
    ncplane_putstr(p, "] │");

    ncplane_putstr_yx(p, ry + 2, bx + 3, "└");
    for (int d = 0; d < field_w + 4; d++) ncplane_putstr(p, "─");
    ncplane_putstr(p, "┘");

    size_t len = strlen(buf);
    const char* display = buf;
    int visible = (int)len;
    if (visible > field_w) {
        display = buf + (len - (size_t)field_w);
        visible = field_w;
    }
    ncplane_set_fg_rgb(p, 0x00E478);
    ncplane_putstr_yx(p, ry + 1, bx + 7, display);

    if (active) {
        ncplane_set_bg_rgb(p, 0x00E478);
        ncplane_set_fg_rgb(p, 0x121221);
        ncplane_putchar_yx(p, ry + 1, bx + 7 + visible, ' ');
        ncplane_set_bg_rgb(p, 0x1E1E2E);
        ncplane_set_fg_rgb(p, 0x00E478);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// /proc/meminfo reader — returns used% (0-100), or -1 on failure
// ─────────────────────────────────────────────────────────────────────────────
static int read_mem_percent(unsigned long* out_used_mb, unsigned long* out_total_mb) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    unsigned long total = 0, free_ = 0, buffers = 0, cached = 0, sreclaimable = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        unsigned long val = 0;
        if      (sscanf(line, "MemTotal: %lu kB",      &val) == 1) total        = val;
        else if (sscanf(line, "MemFree: %lu kB",       &val) == 1) free_        = val;
        else if (sscanf(line, "Buffers: %lu kB",       &val) == 1) buffers      = val;
        else if (sscanf(line, "Cached: %lu kB",        &val) == 1) cached       = val;
        else if (sscanf(line, "SReclaimable: %lu kB",  &val) == 1) sreclaimable = val;
    }
    fclose(f);
    if (total == 0) return -1;
    unsigned long available = free_ + buffers + cached + sreclaimable;
    unsigned long used = (total > available) ? (total - available) : 0;
    *out_total_mb = total / 1024;
    *out_used_mb  = used  / 1024;
    return (int)(used * 100 / total);
}

// ─────────────────────────────────────────────────────────────────────────────
// ASCII art for "SUBASH" — 6 rows tall, each letter 7 cols wide + 1 gap
// ─────────────────────────────────────────────────────────────────────────────
static const char* SUBASH_ART[6][6] = {
    { " ████ ", "██    ", " ███  ", "   ██ ", "████  ", "      " },
    { "██  ██", "██  ██", "██  ██", "██  ██", " ████ ", "      " },
    { "█████ ", "██  ██", "█████ ", "██  ██", "█████ ", "      " },
    { " ████ ", "██  ██", "██████", "██  ██", "██  ██", "      " },
    { " ████ ", "██    ", " ███  ", "   ██ ", "████  ", "      " },
    { "██  ██", "██  ██", "██████", "██  ██", "██  ██", "      " },
};

// Rich multi-hue palette for intro (rainbow sweep, < 300 ms total)
static uint32_t intro_color(int frame, int col_offset, int row) {
    // Fast hue walk across frame + column + row for colorful shimmer
    int t = (frame * 48 + col_offset * 7 + row * 11) % 360;
    if (t < 40)  return 0xFF3366; // hot pink / red
    if (t < 80)  return 0xFF8800; // orange
    if (t < 120) return 0xFFDD00; // gold
    if (t < 160) return 0x00E478; // green
    if (t < 200) return 0x00E4C8; // teal
    if (t < 240) return 0x00B4FF; // cyan-blue
    if (t < 280) return 0x5588FF; // blue
    if (t < 320) return 0xAA66FF; // purple
    return 0xFF66CC;              // magenta
}

// draw_intro: 10 frames × 28 ms = 280 ms total
//   frames 0-5 : reveal one letter each, multi-color from the start
//   frames 6-7 : full-word color sweep + subtitle
//   frames 8-9 : tagline hold then exit
void draw_intro(struct notcurses* nc,
                struct ncplane* left, struct ncplane* right,
                unsigned int full_rows, unsigned int full_cols,
                int intro_frame) {
    (void)nc; (void)right;
    if (full_rows < 4) full_rows = 4;
    if (full_cols < 52) full_cols = 52;
    if (intro_frame < 0) intro_frame = 0;
    if (intro_frame >= INTRO_FRAMES) intro_frame = INTRO_FRAMES - 1;

    ncplane_erase(left);

    // dark bg with subtle colored edges that pulse with frame
    static const uint32_t edge_colors[] = {
        0x1A0A2E, 0x0A1A2E, 0x0A2E1A, 0x2E1A0A, 0x1A2E2E,
        0x2E0A1A, 0x0A0A2E, 0x2E2E0A, 0x1A1A2E, 0x0D0D1A
    };
    uint32_t bg = edge_colors[intro_frame % 10];
    ncplane_set_bg_rgb(left, bg);
    ncplane_set_fg_rgb(left, bg);
    for (unsigned int r = 0; r < full_rows; r++) {
        for (unsigned int c = 0; c < full_cols; c++) {
            ncplane_putchar_yx(left, (int)r, (int)c, ' ');
        }
    }

    const int art_w = 48;
    const int art_h = 6;
    int ay = (int)((full_rows - art_h) / 2) - 1;
    int ax = (int)((full_cols - art_w) / 2);
    if (ay < 1) ay = 1;
    if (ax < 0) ax = 0;

    int letters_visible = intro_frame + 1;
    if (letters_visible > 6) letters_visible = 6;

    for (int li = 0; li < letters_visible; li++) {
        int lx = ax + li * 8;
        for (int row = 0; row < art_h; row++) {
            const char* line = SUBASH_ART[li][row];
            // Per-glyph color from frame one — colorful throughout
            uint32_t color = intro_color(intro_frame, li * 8 + row, row);
            ncplane_set_bg_rgb(left, bg);
            ncplane_set_fg_rgb(left, color);
            ncplane_on_styles(left, NCSTYLE_BOLD);
            ncplane_putstr_yx(left, ay + row, lx, line);
            ncplane_off_styles(left, NCSTYLE_BOLD);
        }
    }

    // colorful underline accent under logo (frames 4+)
    if (intro_frame >= 4) {
        int uy = ay + art_h;
        if (uy < (int)full_rows - 1) {
            for (int c = 0; c < art_w && (ax + c) < (int)full_cols; c++) {
                ncplane_set_fg_rgb(left, intro_color(intro_frame, c, 0));
                ncplane_putstr_yx(left, uy, ax + c, "─");
            }
        }
    }

    // subtitle — frames 6+
    if (intro_frame >= 6) {
        ncplane_set_fg_rgb(left, 0xAADDFF);
        int sub_y = ay + art_h + 1;
        if (sub_y < (int)full_rows - 2) {
            ncplane_putstr_yx(left, sub_y, ax, "  workspace context switcher  ");
        }
    }

    // tagline — frames 8+
    if (intro_frame >= 8) {
        ncplane_set_fg_rgb(left, intro_color(intro_frame, 20, 2));
        int tag_y = ay + art_h + 2;
        if (tag_y < (int)full_rows - 1) {
            ncplane_putstr_yx(left, tag_y, ax, "  v1.0.5  ·  blazing fast  ·  C + notcurses  ");
        }
    }
}

// Truncate/pad a string into dest for fixed-width display
static void fit_str(char* dest, size_t dest_sz, const char* src, int width) {
    if (dest_sz == 0 || width <= 0) return;
    size_t n = strlen(src);
    if ((int)n > width) n = (size_t)width;
    if (n >= dest_sz) n = dest_sz - 1;
    memcpy(dest, src, n);
    // pad
    while ((int)n < width && n + 1 < dest_sz) dest[n++] = ' ';
    dest[n < dest_sz ? n : dest_sz - 1] = '\0';
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

    ncplane_set_bg_rgb(stdplane, 0x121221);
    for (unsigned int r = 0; r < rows; r++)
        for (unsigned int c = 0; c < cols; c++)
            ncplane_putchar_yx(stdplane, (int)r, (int)c, ' ');

    // top bar
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_on_styles(stdplane, NCSTYLE_BOLD);
    ncplane_putstr_yx(stdplane, 0, 2, "~SYSTEM");
    ncplane_off_styles(stdplane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb(stdplane, 0x585B70);
    // focus indicator
    if (state->nav_focus == NAV_FOCUS_PROJECT) {
        ncplane_set_fg_rgb(stdplane, 0x89B4FA);
        ncplane_putstr_yx(stdplane, 0, 28, "[ PROJECTS  ◀ ]");
    } else {
        ncplane_set_fg_rgb(stdplane, 0x00E478);
        ncplane_putstr_yx(stdplane, 0, 28, "[ PRODUCTS  ▶ ]");
    }
    ncplane_set_fg_rgb(stdplane, 0x585B70);
    for (unsigned int c = 0; c < cols; c++)
        ncplane_putstr_yx(stdplane, 1, (int)c, "─");

    // footer
    int footer_y = (int)rows - 1;
    ncplane_set_fg_rgb(stdplane, 0x585B70);
    for (unsigned int c = 0; c < cols; c++)
        ncplane_putstr_yx(stdplane, footer_y - 1, (int)c, "─");

    ncplane_set_fg_rgb(stdplane, 0x888888);
    ncplane_putstr_yx(stdplane, footer_y, 2, "");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[a]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Add  ");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[n]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " New Proj  ");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[e]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Edit  ");
    ncplane_set_fg_rgb(stdplane, 0xFF6B6B);
    ncplane_putstr(stdplane, "[d]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Del  ");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[←/→]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Proj/Prod  ");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[j/k]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Nav  ");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[q]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Quit");

    int version_x = (int)cols - 14;
    if (version_x > 60) {
        ncplane_set_fg_rgb(stdplane, 0x00E478);
        ncplane_putstr_yx(stdplane, footer_y, version_x, "v1.0.5");
    }

    // ── Left pane (compact project list) ─────────────────────────────────────
    ncplane_erase(left);
    ncplane_set_bg_rgb(left, 0x121221);
    unsigned int l_rows, l_cols;
    ncplane_dim_yx(left, &l_rows, &l_cols);

    bool proj_focus = (state->nav_focus == NAV_FOCUS_PROJECT);
    draw_panel_border(left, proj_focus ? 0x89B4FA : 0x585B70, "");

    int nav_y = 1;
    ncplane_set_fg_rgb(left, proj_focus ? 0x89B4FA : 0x888888);
    ncplane_putstr_yx(left, nav_y, 1, "┌─PROJ");
    unsigned int label_len = 6;
    if (l_cols > label_len + 2) {
        ncplane_set_fg_rgb(left, 0x585B70);
        for (unsigned int c = label_len + 1; c < l_cols - 2; c++)
            ncplane_putstr_yx(left, nav_y, (int)c, "─");
        ncplane_putstr_yx(left, nav_y, (int)l_cols - 2, "┐");
    }

    // Compact: only show project names, capped height; leave room below for hint
    int max_proj_rows = (int)l_rows - 4;
    if (max_proj_rows < 1) max_proj_rows = 1;
    // Prefer showing around the selected project if list is long
    int start_pi = 0;
    if (state->project_count > max_proj_rows) {
        start_pi = state->selected_project - max_proj_rows / 2;
        if (start_pi < 0) start_pi = 0;
        if (start_pi + max_proj_rows > state->project_count)
            start_pi = state->project_count - max_proj_rows;
    }

    int row = nav_y + 1;
    for (int i = start_pi; i < state->project_count && (row - nav_y - 1) < max_proj_rows; i++) {
        const Project* proj = &state->projects[i];
        bool is_active = (i == state->selected_project);
        int name_w = (int)l_cols - 5;
        if (name_w < 1) name_w = 1;
        char nbuf[64];
        fit_str(nbuf, sizeof(nbuf), proj->name, name_w > 60 ? 60 : name_w);

        if (is_active) {
            ncplane_set_fg_rgb(left, proj_focus ? 0x00E478 : 0x89B4FA);
            ncplane_putstr_yx(left, row, 1, proj_focus ? "❯" : "·");
            ncplane_set_fg_rgb(left, 0x89B4FA);
            ncplane_on_styles(left, NCSTYLE_BOLD);
            ncplane_printf_yx(left, row, 3, "%s", nbuf);
            ncplane_off_styles(left, NCSTYLE_BOLD);
        } else {
            ncplane_set_fg_rgb(left, 0x556677);
            ncplane_putstr_yx(left, row, 1, " ");
            ncplane_printf_yx(left, row, 3, "%s", nbuf);
        }
        row++;
    }

    // tiny footer hint in left pane
    if ((int)l_rows > 2) {
        ncplane_set_fg_rgb(left, 0x444455);
        ncplane_putstr_yx(left, (int)l_rows - 2, 1, "←→");
    }

    // ── Right pane (products of selected project only) ────────────────────────
    ncplane_erase(right);
    ncplane_set_bg_rgb(right, 0x121221);
    unsigned int r_rows, r_cols;
    ncplane_dim_yx(right, &r_rows, &r_cols);

    bool prod_focus = (state->nav_focus == NAV_FOCUS_PRODUCT);
    draw_panel_border(right, prod_focus ? 0x00E478 : 0x585B70, "");

    const char* sp_name = (state->selected_project >= 0 &&
                           state->selected_project < state->project_count)
                          ? state->projects[state->selected_project].name : "—";

    ncplane_set_fg_rgb(right, 0xE3E0F7);
    ncplane_on_styles(right, NCSTYLE_BOLD);
    ncplane_putstr_yx(right, 0, 2, "┌─ PROCESS REGISTRY");
    ncplane_off_styles(right, NCSTYLE_BOLD);
    ncplane_set_fg_rgb(right, 0x89B4FA);
    ncplane_printf(right, "  %s ", sp_name);

    int prod_count = 0;
    if (state->selected_project >= 0 && state->selected_project < state->project_count)
        prod_count = state->projects[state->selected_project].product_count;

    ncplane_set_fg_rgb(right, 0x556677);
    ncplane_printf_yx(right, 0, (int)r_cols > 40 ? (int)r_cols - 22 : 28,
                      " %02d products ", prod_count);

    // Wider COMPONENT column (28 chars)
    const int COL_ID    = 3;
    const int COL_NAME  = 8;
    const int NAME_W    = 28;
    const int COL_STAT  = COL_NAME + NAME_W + 2;   // 38
    const int COL_PATH  = COL_STAT + 12;            // 50

    int table_y = 2;
    ncplane_set_fg_rgb(right, 0x888888);
    ncplane_on_styles(right, NCSTYLE_BOLD);
    ncplane_printf_yx(right, table_y, 2, "%-4s", "ID");
    ncplane_printf_yx(right, table_y, COL_NAME, "%-*s", NAME_W, "COMPONENT");
    ncplane_printf_yx(right, table_y, COL_STAT, "%-10s", "STATUS");
    if (COL_PATH < (int)r_cols - 2)
        ncplane_printf_yx(right, table_y, COL_PATH, "%s", "PATH / NOTES");
    ncplane_off_styles(right, NCSTYLE_BOLD);
    ncplane_set_fg_rgb(right, 0x585B70);
    for (unsigned int c = 2; c < r_cols - 1; c++)
        ncplane_putstr_yx(right, table_y + 1, (int)c, "─");

    int py        = table_y + 2;
    int table_max = (int)r_rows - 5;
    if (table_max < 1) table_max = 1;

    if (state->selected_project >= 0 && state->selected_project < state->project_count) {
        const Project* proj = &state->projects[state->selected_project];
        for (int j = 0; j < proj->product_count; j++) {
            if (py >= table_y + 2 + table_max) break;
            const Product* prod = &proj->products[j];
            bool selected = (j == state->selected_product) && prod_focus;
            bool marked   = (j == state->selected_product);

            if (selected) {
                ncplane_set_fg_rgb(right, 0x00E478);
                ncplane_putstr_yx(right, py, 1, "❯");
                ncplane_on_styles(right, NCSTYLE_BOLD);
            } else if (marked) {
                ncplane_set_fg_rgb(right, 0x89B4FA);
                ncplane_putstr_yx(right, py, 1, "·");
            } else {
                ncplane_putstr_yx(right, py, 1, " ");
            }

            ncplane_set_fg_rgb(right, 0x888888);
            ncplane_printf_yx(right, py, COL_ID, "%03d", j + 1);

            char name_disp[64];
            fit_str(name_disp, sizeof(name_disp), prod->name, NAME_W);
            ncplane_set_fg_rgb(right, selected ? 0x89B4FA : (marked ? 0xCDB4FF : 0xE3E0F7));
            ncplane_printf_yx(right, py, COL_NAME, "%s", name_disp);

            if (selected || marked) {
                ncplane_set_fg_rgb(right, 0x00E478);
                ncplane_printf_yx(right, py, COL_STAT, "[ ACTIVE ]");
            } else {
                switch (j % 4) {
                    case 0: ncplane_set_fg_rgb(right, 0x00E478);
                            ncplane_printf_yx(right, py, COL_STAT, "[ ACTIVE ]"); break;
                    case 1: ncplane_set_fg_rgb(right, 0x89B4FA);
                            ncplane_printf_yx(right, py, COL_STAT, "[ NOMINAL]"); break;
                    case 2: ncplane_set_fg_rgb(right, 0xFFFF00);
                            ncplane_printf_yx(right, py, COL_STAT, "[ TUNING ]"); break;
                    default:ncplane_set_fg_rgb(right, 0xFF4444);
                            ncplane_printf_yx(right, py, COL_STAT, "[CRITICAL]"); break;
                }
            }

            // path, fall back to notes snippet
            ncplane_set_fg_rgb(right, 0x556677);
            int path_w = (int)r_cols - COL_PATH - 2;
            if (path_w > 0 && COL_PATH < (int)r_cols - 2) {
                const char* dp = prod->local_path[0] ? prod->local_path
                                 : (prod->notes[0] ? prod->notes : "");
                size_t pln = strlen(dp);
                if ((int)pln > path_w && path_w > 2) {
                    dp = dp + (pln - (size_t)path_w + 2);
                    ncplane_printf_yx(right, py, COL_PATH, "..%s", dp);
                } else if (dp[0]) {
                    ncplane_printf_yx(right, py, COL_PATH, "%s", dp);
                }
            }

            if (selected) ncplane_off_styles(right, NCSTYLE_BOLD);
            py++;
        }
    }

    while (py < table_y + 2 + table_max) {
        ncplane_set_fg_rgb(right, 0x333344);
        ncplane_printf_yx(right, py, COL_ID, "%03d", (py - table_y - 1));
        ncplane_putstr_yx(right, py, COL_NAME,
            " | ............................................");
        py++;
    }

    // ── Stats bar ─────────────────────────────────────────────────────────────
    ncplane_set_fg_rgb(right, 0x585B70);
    for (unsigned int c = 2; c < r_cols - 1; c++)
        ncplane_putstr_yx(right, (int)r_rows - 3, (int)c, "─");

    int stats_y = (int)r_rows - 2;
    unsigned long used_mb = 0, total_mb = 0;
    int mem_pct = read_mem_percent(&used_mb, &total_mb);

    ncplane_set_fg_rgb(right, 0x888888);
    ncplane_putstr_yx(right, stats_y, 2, "MEM: ");

    if (mem_pct >= 0) {
        int filled = (mem_pct * 10 + 50) / 100;
        if (filled > 10) filled = 10;
        uint32_t bar_color = (mem_pct < 60) ? 0x89B4FA :
                             (mem_pct < 80) ? 0xFFFF00 : 0xFF4444;
        ncplane_set_fg_rgb(right, bar_color);
        for (int b = 0; b < 10; b++)
            ncplane_putstr(right, b < filled ? "█" : "░");
        ncplane_set_fg_rgb(right, 0x888888);
        ncplane_printf(right, " %d%%  %luMB/%luMB   ", mem_pct, used_mb, total_mb);
    } else {
        ncplane_set_fg_rgb(right, 0x585B70);
        ncplane_putstr(right, "unavailable");
    }

    // selected product tasks/links peek
    Product* sel = get_selected_product(state, NULL);
    if (sel && (int)r_cols > 50) {
        int peek_x = (int)r_cols / 2;
        ncplane_set_fg_rgb(right, 0x666688);
        if (sel->tasks[0]) {
            char tbuf[40];
            fit_str(tbuf, sizeof(tbuf), sel->tasks, 36);
            ncplane_printf_yx(right, stats_y, peek_x, "tasks: %s", tbuf);
        } else if (sel->link_count > 0) {
            char tbuf[40];
            fit_str(tbuf, sizeof(tbuf), sel->links[0], 36);
            ncplane_printf_yx(right, stats_y, peek_x, "link: %s", tbuf);
        }
    }

    (void)frame_tick;
}

// ─────────────────────────────────────────────────────────────────────────────
// Common form background + top bar helper
// ─────────────────────────────────────────────────────────────────────────────
static void fill_form_bg(struct ncplane* p) {
    unsigned int rows, cols;
    ncplane_dim_yx(p, &rows, &cols);
    ncplane_set_bg_rgb(p, 0x121221);
    for (unsigned int r = 0; r < rows; r++)
        for (unsigned int c = 0; c < cols; c++)
            ncplane_putchar_yx(p, (int)r, (int)c, ' ');
    ncplane_set_fg_rgb(p, 0x00E478);
    ncplane_on_styles(p, NCSTYLE_BOLD);
    ncplane_putstr_yx(p, 0, 2, "⚡ SYSTEM::DASHBOARD");
    ncplane_off_styles(p, NCSTYLE_BOLD);
    ncplane_set_fg_rgb(p, 0x585B70);
    for (unsigned int c = 0; c < cols; c++)
        ncplane_putstr_yx(p, 1, (int)c, "─");
}

static void draw_form_footer(struct ncplane* p, const char* hints) {
    unsigned int rows, cols;
    ncplane_dim_yx(p, &rows, &cols);
    int fy = (int)rows - 1;
    ncplane_set_fg_rgb(p, 0x585B70);
    for (unsigned int c = 0; c < cols; c++)
        ncplane_putstr_yx(p, fy - 1, (int)c, "─");
    ncplane_set_fg_rgb(p, 0x888888);
    ncplane_putstr_yx(p, fy, 2, hints);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw Form (MODE_FORM) — add product or add project
// ─────────────────────────────────────────────────────────────────────────────
void draw_form(struct notcurses* nc, struct ncplane* std_plane,
               const FormState* form, const AppState* state) {
    (void)nc;
    fill_form_bg(std_plane);

    unsigned int rows, cols;
    ncplane_dim_yx(std_plane, &rows, &cols);

    bool is_product = (form->form_mode == FORM_MODE_ADD_PRODUCT);
    // product form: 6 fields × 3 rows + header/buttons ≈ 26
    unsigned int box_h = is_product ? 26 : 10;
    unsigned int box_w = 72;
    if (box_w > cols) box_w = cols;
    if (box_h > rows - 2) box_h = rows > 2 ? rows - 2 : rows;
    unsigned int box_y = (rows > box_h) ? (rows - box_h) / 2 : 1;
    unsigned int box_x = (cols > box_w) ? (cols - box_w) / 2 : 0;

    ncplane_set_bg_rgb(std_plane, 0x1E1E2E);
    for (unsigned int r = box_y; r < box_y + box_h; r++)
        for (unsigned int c = box_x; c < box_x + box_w; c++)
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');

    draw_panel_border(std_plane, 0x585B70, is_product ? "ADD_PRODUCT" : "NEW_PROJECT");

    ncplane_set_fg_rgb(std_plane, 0x00E478);
    ncplane_putstr_yx(std_plane, (int)box_y + 1, (int)box_x + 3, "[ INFO ]");
    ncplane_set_fg_rgb(std_plane, 0xE3E0F7);
    if (is_product) {
        ncplane_printf(std_plane, " New product — set project name (existing or new)");
    } else {
        ncplane_printf(std_plane, " Create a new project workspace");
    }

    int field_w = (int)box_w - 12;
    if (field_w < 4) field_w = 4;

    if (is_product) {
        // Project Name, Product Name, Path, Tasks, Links, Notes
        int fy = (int)box_y + 3;
        draw_input_field(std_plane, "Project Name",
                         form->new_project, field_w,
                         form->active_field == FORM_FIELD_PROJECT,
                         (int)box_x, fy);
        fy += 3;
        draw_input_field(std_plane, "Product Name",
                         form->new_name, field_w,
                         form->active_field == FORM_FIELD_NAME,
                         (int)box_x, fy);
        fy += 3;
        draw_input_field(std_plane, "Local Path",
                         form->new_path, field_w,
                         form->active_field == FORM_FIELD_PATH,
                         (int)box_x, fy);
        fy += 3;
        draw_input_field(std_plane, "Tasks",
                         form->new_tasks, field_w,
                         form->active_field == FORM_FIELD_TASKS,
                         (int)box_x, fy);
        fy += 3;
        draw_input_field(std_plane, "Links",
                         form->new_links, field_w,
                         form->active_field == FORM_FIELD_LINKS,
                         (int)box_x, fy);
        fy += 3;
        draw_input_field(std_plane, "Notes",
                         form->new_notes, field_w,
                         form->active_field == FORM_FIELD_NOTES,
                         (int)box_x, fy);

        // hint: existing projects
        if (state->project_count > 0 && (int)box_y + (int)box_h - 4 > fy + 3) {
            ncplane_set_fg_rgb(std_plane, 0x556677);
            ncplane_putstr_yx(std_plane, fy + 3, (int)box_x + 3, "projects:");
            for (int i = 0; i < state->project_count && i < 6; i++) {
                bool match = (strcmp(state->projects[i].name, form->new_project) == 0);
                ncplane_set_fg_rgb(std_plane, match ? 0x00E478 : 0x666688);
                ncplane_printf(std_plane, " %s", state->projects[i].name);
            }
        }
    } else {
        draw_input_field(std_plane, "Project Name",
                         form->new_name, field_w,
                         form->active_field == FORM_FIELD_NAME,
                         (int)box_x, (int)box_y + 4);
    }

    int br = (int)box_y + (int)box_h - 3;
    ncplane_set_fg_rgb(std_plane, 0x00E478);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 3, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 3, "│  [SAVE]  │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 3, "└──────────┘");
    int cancel_x = (int)box_w > 20 ? (int)box_x + (int)box_w - 14 : (int)box_x + 40;
    ncplane_set_fg_rgb(std_plane, 0x585B70);
    ncplane_putstr_yx(std_plane, br,     cancel_x, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, cancel_x, "│ [CANCEL] │");
    ncplane_putstr_yx(std_plane, br + 2, cancel_x, "└──────────┘");

    if (form->error_msg[0] != '\0') {
        ncplane_set_fg_rgb(std_plane, 0xFF4444);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf_yx(std_plane, br - 1, (int)box_x + 3, "! %s", form->error_msg);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
    }

    draw_form_footer(std_plane,
        is_product ? " [Tab/↑↓] Field   [Enter] Next/Save   [Esc] Cancel"
                   : " [Enter] Save New Project   [Esc] Cancel");
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw Confirm (MODE_CONFIRM)
// ─────────────────────────────────────────────────────────────────────────────
void draw_confirm(struct notcurses* nc, struct ncplane* std_plane,
                  const ConfirmState* confirm, const AppState* state) {
    (void)nc; (void)state;
    fill_form_bg(std_plane);

    unsigned int rows, cols;
    ncplane_dim_yx(std_plane, &rows, &cols);

    unsigned int box_h = 9, box_w = 58;
    if (box_w > cols) box_w = cols;
    if (box_h > rows) box_h = rows;
    unsigned int box_y = (rows > box_h) ? (rows - box_h) / 2 : 0;
    unsigned int box_x = (cols > box_w) ? (cols - box_w) / 2 : 0;

    ncplane_set_bg_rgb(std_plane, 0x1E1E2E);
    for (unsigned int r = box_y; r < box_y + box_h; r++)
        for (unsigned int c = box_x; c < box_x + box_w; c++)
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');

    draw_panel_border(std_plane, 0xFF4444, "CONFIRM_DELETE");

    ncplane_set_fg_rgb(std_plane, 0xFF6B6B);
    ncplane_on_styles(std_plane, NCSTYLE_BOLD);
    ncplane_putstr_yx(std_plane, (int)box_y + 2, (int)box_x + 3, "⚠  WARNING: This action cannot be undone.");
    ncplane_off_styles(std_plane, NCSTYLE_BOLD);

    ncplane_set_fg_rgb(std_plane, 0xE3E0F7);
    if (confirm->action == CONFIRM_DELETE_PRODUCT) {
        ncplane_printf_yx(std_plane, (int)box_y + 3, (int)box_x + 3,
                          "Delete product: ");
        ncplane_set_fg_rgb(std_plane, 0x89B4FA);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf(std_plane, "%s", confirm->subject_name);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
    } else {
        ncplane_printf_yx(std_plane, (int)box_y + 3, (int)box_x + 3,
                          "Delete project: ");
        ncplane_set_fg_rgb(std_plane, 0xFF6B6B);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf(std_plane, "%s", confirm->subject_name);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
        ncplane_set_fg_rgb(std_plane, 0xE3E0F7);
        ncplane_printf_yx(std_plane, (int)box_y + 4, (int)box_x + 3,
                          "(all products in this project will also be removed)");
    }

    int br = (int)box_y + (int)box_h - 3;
    ncplane_set_fg_rgb(std_plane, 0xFF4444);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 3,  "┌────────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 3,  "│  [y] YES   │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 3,  "└────────────┘");
    ncplane_set_fg_rgb(std_plane, 0x00E478);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 39, "┌────────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 39, "│  [n] NO    │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 39, "└────────────┘");

    draw_form_footer(std_plane, " [y] Confirm Delete   [n / Esc] Cancel");
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw Edit (MODE_EDIT)
// ─────────────────────────────────────────────────────────────────────────────
void draw_edit(struct notcurses* nc, struct ncplane* std_plane,
               const EditState* edit, const AppState* state) {
    (void)nc; (void)state;
    fill_form_bg(std_plane);

    unsigned int rows, cols;
    ncplane_dim_yx(std_plane, &rows, &cols);

    bool is_product = (edit->target == EDIT_TARGET_PRODUCT);
    unsigned int box_h = is_product ? 22 : 10;
    unsigned int box_w = 72;
    if (box_w > cols) box_w = cols;
    if (box_h > rows - 2) box_h = rows > 2 ? rows - 2 : rows;
    unsigned int box_y = (rows > box_h) ? (rows - box_h) / 2 : 1;
    unsigned int box_x = (cols > box_w) ? (cols - box_w) / 2 : 0;

    ncplane_set_bg_rgb(std_plane, 0x1E1E2E);
    for (unsigned int r = box_y; r < box_y + box_h; r++)
        for (unsigned int c = box_x; c < box_x + box_w; c++)
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');

    draw_panel_border(std_plane, 0x89B4FA,
                      is_product ? "EDIT_PRODUCT" : "EDIT_PROJECT");

    ncplane_set_fg_rgb(std_plane, 0x89B4FA);
    ncplane_putstr_yx(std_plane, (int)box_y + 1, (int)box_x + 3, "[ EDIT ]");
    ncplane_set_fg_rgb(std_plane, 0xE3E0F7);
    ncplane_printf(std_plane, is_product ? " Modify product details"
                                         : " Rename project");

    int field_w = (int)box_w - 12;
    if (field_w < 4) field_w = 4;

    if (is_product) {
        int fy = (int)box_y + 3;
        draw_input_field(std_plane, "Product Name",
                         edit->name_buf, field_w,
                         edit->active_field == FORM_FIELD_NAME,
                         (int)box_x, fy); fy += 3;
        draw_input_field(std_plane, "Local Path",
                         edit->path_buf, field_w,
                         edit->active_field == FORM_FIELD_PATH,
                         (int)box_x, fy); fy += 3;
        draw_input_field(std_plane, "Tasks",
                         edit->tasks_buf, field_w,
                         edit->active_field == FORM_FIELD_TASKS,
                         (int)box_x, fy); fy += 3;
        draw_input_field(std_plane, "Links",
                         edit->links_buf, field_w,
                         edit->active_field == FORM_FIELD_LINKS,
                         (int)box_x, fy); fy += 3;
        draw_input_field(std_plane, "Notes",
                         edit->notes_buf, field_w,
                         edit->active_field == FORM_FIELD_NOTES,
                         (int)box_x, fy);
    } else {
        draw_input_field(std_plane, "Project Name",
                         edit->name_buf, field_w,
                         edit->active_field == FORM_FIELD_NAME,
                         (int)box_x, (int)box_y + 4);
    }

    int br = (int)box_y + (int)box_h - 3;
    ncplane_set_fg_rgb(std_plane, 0x89B4FA);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 3, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 3, "│  [SAVE]  │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 3, "└──────────┘");
    int cancel_x = (int)box_w > 20 ? (int)box_x + (int)box_w - 14 : (int)box_x + 40;
    ncplane_set_fg_rgb(std_plane, 0x585B70);
    ncplane_putstr_yx(std_plane, br,     cancel_x, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, cancel_x, "│ [CANCEL] │");
    ncplane_putstr_yx(std_plane, br + 2, cancel_x, "└──────────┘");

    if (edit->error_msg[0] != '\0') {
        ncplane_set_fg_rgb(std_plane, 0xFF4444);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf_yx(std_plane, br - 1, (int)box_x + 3, "! %s", edit->error_msg);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
    }

    draw_form_footer(std_plane,
        is_product ? " [Tab/↑↓] Switch Field   [Enter] Save   [Esc] Cancel"
                   : " [Enter] Save   [Esc] Cancel");
}
