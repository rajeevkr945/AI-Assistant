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

    // top border with label
    ncplane_putstr_yx(p, ry, bx + 3, "┌─ ");
    ncplane_putstr(p, label);
    int dashes = field_w - (int)strlen(label) - 1;
    if (dashes < 0) dashes = 0;
    for (int d = 0; d < dashes; d++) ncplane_putstr(p, "─");
    ncplane_putstr(p, "┐");

    // middle row
    ncplane_putstr_yx(p, ry + 1, bx + 3, "│ [");
    for (int d = 0; d < field_w; d++) ncplane_putstr(p, " ");
    ncplane_putstr(p, "] │");

    // bottom border
    ncplane_putstr_yx(p, ry + 2, bx + 3, "└");
    for (int d = 0; d < field_w + 4; d++) ncplane_putstr(p, "─");
    ncplane_putstr(p, "┘");

    // value with scroll: always show tail so cursor is visible
    size_t len = strlen(buf);
    const char* display = buf;
    int visible = (int)len;
    if (visible > field_w) {
        display = buf + (len - (size_t)field_w);
        visible = field_w;
    }
    ncplane_set_fg_rgb(p, 0x00E478);
    ncplane_putstr_yx(p, ry + 1, bx + 7, display);

    // cursor
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
// Total width: 6 letters × 8 = 48 cols
// ─────────────────────────────────────────────────────────────────────────────
static const char* SUBASH_ART[6][6] = {
    // S
    { " ████ ",
      "██    ",
      " ███  ",
      "   ██ ",
      "████  ",
      "      " },
    // U
    { "██  ██",
      "██  ██",
      "██  ██",
      "██  ██",
      " ████ ",
      "      " },
    // B
    { "█████ ",
      "██  ██",
      "█████ ",
      "██  ██",
      "█████ ",
      "      " },
    // A
    { " ████ ",
      "██  ██",
      "██████",
      "██  ██",
      "██  ██",
      "      " },
    // S
    { " ████ ",
      "██    ",
      " ███  ",
      "   ██ ",
      "████  ",
      "      " },
    // H
    { "██  ██",
      "██  ██",
      "██████",
      "██  ██",
      "██  ██",
      "      " },
};

// Color palette cycling through intro frames (green → cyan → blue → purple)
static uint32_t intro_color(int frame, int col_offset) {
    // hue shifts based on frame + column to create a sweep effect
    int t = (frame * 12 + col_offset * 3) % 360;
    if (t < 60)  return 0x00E478; // green
    if (t < 120) return 0x00E4C8; // teal
    if (t < 180) return 0x00B4E4; // cyan
    if (t < 240) return 0x5588FF; // blue
    if (t < 300) return 0xAA66FF; // purple
    return 0x00E478;              // back to green
}

// draw_intro: 20 frames
//   frames  0- 5: each frame reveals one more letter (char-by-char within letter)
//   frames  6-13: color sweep across all letters
//   frames 14-17: subtitle + tagline fade-in
//   frames 18-19: "loading…" blink + hold
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

    // solid dark background
    ncplane_set_bg_rgb(left, 0x0D0D1A);
    ncplane_set_fg_rgb(left, 0x0D0D1A);
    for (unsigned int r = 0; r < full_rows; r++) {
        for (unsigned int c = 0; c < full_cols; c++) {
            ncplane_putchar_yx(left, (int)r, (int)c, ' ');
        }
    }

    // art dimensions: 6 letters × 8 cols each (6 chars + 2 gap) = 48 wide, 6 tall
    const int art_w = 48;
    const int art_h = 6;
    int ay = (int)((full_rows - art_h) / 2) - 1;
    int ax = (int)((full_cols - art_w) / 2);
    if (ay < 1) ay = 1;
    if (ax < 0) ax = 0;

    // how many letters to reveal: frames 0-5 → 1 letter per frame
    int letters_visible = intro_frame + 1;
    if (letters_visible > 6) letters_visible = 6;

    for (int li = 0; li < letters_visible; li++) {
        int lx = ax + li * 8;
        for (int row = 0; row < art_h; row++) {
            const char* line = SUBASH_ART[li][row];
            int col_offset   = li * 8;
            // color: sweep in frames 6+
            uint32_t color = (intro_frame >= 6)
                             ? intro_color(intro_frame, col_offset)
                             : 0x00E478;
            ncplane_set_fg_rgb(left, color);
            ncplane_on_styles(left, NCSTYLE_BOLD);
            ncplane_putstr_yx(left, ay + row, lx, line);
            ncplane_off_styles(left, NCSTYLE_BOLD);
        }
    }

    // subtitle — visible from frame 14 onward
    if (intro_frame >= 14) {
        ncplane_set_fg_rgb(left, 0x888888);
        int sub_y = ay + art_h + 1;
        if (sub_y < (int)full_rows - 2) {
            ncplane_putstr_yx(left, sub_y, ax, "  workspace context switcher  ");
        }
    }

    // tagline — visible from frame 16 onward
    if (intro_frame >= 16) {
        ncplane_set_fg_rgb(left, 0x585B70);
        int tag_y = ay + art_h + 2;
        if (tag_y < (int)full_rows - 1) {
            ncplane_putstr_yx(left, tag_y, ax, "  v1.0.4  ·  blazing fast  ·  C + notcurses  ");
        }
    }

    // "loading…" blink — frames 18-19
    if (intro_frame >= 18) {
        ncplane_set_fg_rgb(left, 0x00E478);
        int ld_y = ay + art_h + 4;
        if (ld_y < (int)full_rows) {
            ncplane_putstr_yx(left, ld_y, ax + 10, "[ loading system dashboard... ]");
        }
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

    // full background
    ncplane_set_bg_rgb(stdplane, 0x121221);
    for (unsigned int r = 0; r < rows; r++)
        for (unsigned int c = 0; c < cols; c++)
            ncplane_putchar_yx(stdplane, (int)r, (int)c, ' ');

    // top bar
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_on_styles(stdplane, NCSTYLE_BOLD);
    ncplane_putstr_yx(stdplane, 0, 2, "⚡ SYSTEM::DASHBOARD");
    ncplane_off_styles(stdplane, NCSTYLE_BOLD);
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
    ncplane_putstr(stdplane, " Del Product  ");
    ncplane_set_fg_rgb(stdplane, 0xFF6B6B);
    ncplane_putstr(stdplane, "[D]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Del Project  ");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[j/k]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Nav  ");
    ncplane_set_fg_rgb(stdplane, 0x00E478);
    ncplane_putstr(stdplane, "[q]");
    ncplane_set_fg_rgb(stdplane, 0xE3E0F7);
    ncplane_putstr(stdplane, " Quit");

    // version
    int version_x = (int)cols - 20;
    if (version_x > 50) {
        ncplane_set_fg_rgb(stdplane, 0x00E478);
        ncplane_putstr_yx(stdplane, footer_y, version_x, "v1.0.4-stable");
    }

    // ── Left pane (sidebar) ──────────────────────────────────────────────────
    ncplane_erase(left);
    ncplane_set_bg_rgb(left, 0x121221);
    unsigned int l_rows, l_cols;
    ncplane_dim_yx(left, &l_rows, &l_cols);
    draw_panel_border(left, 0x585B70, "");

    int nav_y = 1;
    ncplane_set_fg_rgb(left, 0x888888);
    ncplane_putstr_yx(left, nav_y, 1, "┌─ NAVIGATION");
    unsigned int label_len = 13;
    if (l_cols > label_len + 2) {
        ncplane_set_fg_rgb(left, 0x585B70);
        for (unsigned int c = label_len + 1; c < l_cols - 2; c++)
            ncplane_putstr_yx(left, nav_y, (int)c, "─");
        ncplane_putstr_yx(left, nav_y, (int)l_cols - 2, "┐");
    }

    const Project* sel_proj = NULL;
    get_product_at(state, state->selected_index, &sel_proj);

    int row = nav_y + 1;
    for (int i = 0; i < state->project_count; i++) {
        if (row >= (int)l_rows - 2) break;
        const Project* proj = &state->projects[i];
        bool is_active = (sel_proj && strcmp(proj->name, sel_proj->name) == 0);
        if (is_active) {
            ncplane_set_fg_rgb(left, 0x00E478);
            ncplane_putstr_yx(left, row, 2, "❯ ");
            ncplane_set_fg_rgb(left, 0x89B4FA);
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

    // ── Right pane ────────────────────────────────────────────────────────────
    ncplane_erase(right);
    ncplane_set_bg_rgb(right, 0x121221);
    unsigned int r_rows, r_cols;
    ncplane_dim_yx(right, &r_rows, &r_cols);
    draw_panel_border(right, 0x585B70, "");

    ncplane_set_fg_rgb(right, 0xE3E0F7);
    ncplane_on_styles(right, NCSTYLE_BOLD);
    ncplane_putstr_yx(right, 0, 2, "┌─ PROCESS REGISTRY ──────┐");
    ncplane_off_styles(right, NCSTYLE_BOLD);

    int total_prods = get_total_product_count(state);
    ncplane_set_fg_rgb(right, 0x556677);
    ncplane_printf_yx(right, 0, 30, " Listing: %02d total nodes ", total_prods);

    // table header
    int table_y = 2;
    ncplane_set_fg_rgb(right, 0x888888);
    ncplane_on_styles(right, NCSTYLE_BOLD);
    ncplane_printf_yx(right, table_y, 2, "%-4s   %-15s   %-12s   %-10s   %s",
                      "ID", "COMPONENT NAME", "PROJECT", "STATUS", "PATH");
    ncplane_off_styles(right, NCSTYLE_BOLD);
    ncplane_set_fg_rgb(right, 0x585B70);
    for (unsigned int c = 2; c < r_cols - 1; c++)
        ncplane_putstr_yx(right, table_y + 1, (int)c, "─");

    // table rows
    int py          = table_y + 2;
    int table_max   = (int)r_rows - 5;
    int flat_idx    = 0;

    for (int i = 0; i < state->project_count; i++) {
        const Project* proj = &state->projects[i];
        for (int j = 0; j < proj->product_count; j++) {
            if (py >= table_y + 2 + table_max) break;
            const Product* prod = &proj->products[j];
            bool selected = (flat_idx == state->selected_index);

            if (selected) {
                ncplane_set_fg_rgb(right, 0x00E478);
                ncplane_putstr_yx(right, py, 1, "❯");
                ncplane_on_styles(right, NCSTYLE_BOLD);
            } else {
                ncplane_putstr_yx(right, py, 1, " ");
            }

            ncplane_set_fg_rgb(right, 0x888888);
            ncplane_printf_yx(right, py, 3, "%03d", flat_idx + 1);

            ncplane_set_fg_rgb(right, selected ? 0x89B4FA : 0xE3E0F7);
            ncplane_printf_yx(right, py, 8, "%-15s", prod->name);

            ncplane_set_fg_rgb(right, 0x556677);
            ncplane_printf_yx(right, py, 25, "%-12s", proj->name);

            // status
            if (selected) {
                ncplane_set_fg_rgb(right, 0x00E478);
                ncplane_printf_yx(right, py, 39, "[ ACTIVE ]");
            } else {
                switch (flat_idx % 4) {
                    case 0: ncplane_set_fg_rgb(right, 0x00E478);
                            ncplane_printf_yx(right, py, 39, "[ ACTIVE ]"); break;
                    case 1: ncplane_set_fg_rgb(right, 0x89B4FA);
                            ncplane_printf_yx(right, py, 39, "[ NOMINAL]"); break;
                    case 2: ncplane_set_fg_rgb(right, 0xFFFF00);
                            ncplane_printf_yx(right, py, 39, "[ TUNING ]"); break;
                    default:ncplane_set_fg_rgb(right, 0xFF4444);
                            ncplane_printf_yx(right, py, 39, "[CRITICAL]"); break;
                }
            }

            // path (truncated left if long)
            ncplane_set_fg_rgb(right, 0x556677);
            int path_start = 51;
            int path_w     = (int)r_cols - path_start - 2;
            if (path_w > 0) {
                const char* dp  = prod->local_path;
                size_t      pln = strlen(dp);
                if ((int)pln > path_w) {
                    dp = prod->local_path + (pln - (size_t)path_w + 2);
                    ncplane_printf_yx(right, py, path_start, "..%s", dp);
                } else {
                    ncplane_printf_yx(right, py, path_start, "%s", dp);
                }
            }

            if (selected) ncplane_off_styles(right, NCSTYLE_BOLD);
            py++;
            flat_idx++;
        }
    }

    // dotted fill
    while (py < table_y + 2 + table_max) {
        ncplane_set_fg_rgb(right, 0x333344);
        ncplane_printf_yx(right, py, 3, "%03d", flat_idx + 1);
        ncplane_putstr_yx(right, py, 8,
            " | ...................................................................................");
        py++;
        flat_idx++;
    }

    // ── Stats bar ─────────────────────────────────────────────────────────────
    ncplane_set_fg_rgb(right, 0x585B70);
    for (unsigned int c = 2; c < r_cols - 1; c++)
        ncplane_putstr_yx(right, (int)r_rows - 3, (int)c, "─");

    int stats_y = (int)r_rows - 2;

    // Real memory bar
    unsigned long used_mb = 0, total_mb = 0;
    int mem_pct = read_mem_percent(&used_mb, &total_mb);

    ncplane_set_fg_rgb(right, 0x888888);
    ncplane_putstr_yx(right, stats_y, 2, "MEM: ");

    if (mem_pct >= 0) {
        // 10-block bar
        int filled = (mem_pct * 10 + 50) / 100;
        if (filled > 10) filled = 10;
        // color by usage
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

    // search bar / cursor blink
    int search_x = (int)r_cols - 28;
    if (search_x > 35) {
        ncplane_set_fg_rgb(right, 0x888888);
        ncplane_putstr_yx(right, stats_y, search_x, "SEARCH: ");
        ncplane_set_bg_rgb(right, 0x1E1E2E);
        ncplane_set_fg_rgb(right, 0x00E478);
        ncplane_printf(right, " / ");
        ncplane_set_fg_rgb(right, 0xE3E0F7);
        ncplane_printf(right, "_ ");
        if ((frame_tick / 10) % 2 == 0) {
            ncplane_set_bg_rgb(right, 0x00E478);
            ncplane_putstr(right, " ");
        } else {
            ncplane_set_bg_rgb(right, 0x1E1E2E);
            ncplane_putstr(right, " ");
        }
        ncplane_set_bg_rgb(right, 0x121221);
    }
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
    unsigned int box_h = is_product ? 13 : 10;
    unsigned int box_w = 64;
    if (box_w > cols) box_w = cols;
    if (box_h > rows) box_h = rows;
    unsigned int box_y = (rows > box_h) ? (rows - box_h) / 2 : 0;
    unsigned int box_x = (cols > box_w) ? (cols - box_w) / 2 : 0;

    // panel bg
    ncplane_set_bg_rgb(std_plane, 0x1E1E2E);
    for (unsigned int r = box_y; r < box_y + box_h; r++)
        for (unsigned int c = box_x; c < box_x + box_w; c++)
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');

    draw_panel_border(std_plane, 0x585B70, is_product ? "ADD_PRODUCT" : "NEW_PROJECT");

    // info line
    ncplane_set_fg_rgb(std_plane, 0x00E478);
    ncplane_putstr_yx(std_plane, (int)box_y + 2, (int)box_x + 3, "[ INFO ]");
    ncplane_set_fg_rgb(std_plane, 0xE3E0F7);
    if (is_product) {
        const char* pname = (form->target_project_index >= 0 &&
                             form->target_project_index < state->project_count)
                            ? state->projects[form->target_project_index].name : "???";
        ncplane_printf(std_plane, " Adding product to project: ");
        ncplane_set_fg_rgb(std_plane, 0x89B4FA);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf(std_plane, "%s", pname);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
    } else {
        ncplane_printf(std_plane, " Create a new project workspace");
    }

    int field_w = (int)box_w - 12;
    if (field_w < 4) field_w = 4;

    draw_input_field(std_plane, is_product ? "Product Name" : "Project Name",
                     form->new_name, field_w,
                     form->active_field == FORM_FIELD_NAME,
                     (int)box_x, (int)box_y + 4);

    if (is_product) {
        draw_input_field(std_plane, "Local Path",
                         form->new_path, field_w,
                         form->active_field == FORM_FIELD_PATH,
                         (int)box_x, (int)box_y + 7);
    }

    // buttons
    int br = (int)box_y + (int)box_h - 3;
    ncplane_set_fg_rgb(std_plane, 0x00E478);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 3, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 3, "│  [SAVE]  │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 3, "└──────────┘");
    ncplane_set_fg_rgb(std_plane, 0x585B70);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 49, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 49, "│ [CANCEL] │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 49, "└──────────┘");

    if (form->error_msg[0] != '\0') {
        ncplane_set_fg_rgb(std_plane, 0xFF4444);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf_yx(std_plane, br - 1, (int)box_x + 3, "! %s", form->error_msg);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
    }

    draw_form_footer(std_plane,
        is_product ? " [Tab/↑↓] Switch Field   [Enter] Save   [Esc] Cancel"
                   : " [Enter] Save New Project   [Esc] Cancel");
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw Confirm (MODE_CONFIRM) — delete confirmation dialog
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

    // warning icon + message
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

    // buttons
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
// Draw Edit (MODE_EDIT) — edit existing product or project
// ─────────────────────────────────────────────────────────────────────────────
void draw_edit(struct notcurses* nc, struct ncplane* std_plane,
               const EditState* edit, const AppState* state) {
    (void)nc; (void)state;
    fill_form_bg(std_plane);

    unsigned int rows, cols;
    ncplane_dim_yx(std_plane, &rows, &cols);

    bool is_product = (edit->target == EDIT_TARGET_PRODUCT);
    unsigned int box_h = is_product ? 13 : 10;
    unsigned int box_w = 64;
    if (box_w > cols) box_w = cols;
    if (box_h > rows) box_h = rows;
    unsigned int box_y = (rows > box_h) ? (rows - box_h) / 2 : 0;
    unsigned int box_x = (cols > box_w) ? (cols - box_w) / 2 : 0;

    ncplane_set_bg_rgb(std_plane, 0x1E1E2E);
    for (unsigned int r = box_y; r < box_y + box_h; r++)
        for (unsigned int c = box_x; c < box_x + box_w; c++)
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');

    draw_panel_border(std_plane, 0x89B4FA,
                      is_product ? "EDIT_PRODUCT" : "EDIT_PROJECT");

    ncplane_set_fg_rgb(std_plane, 0x89B4FA);
    ncplane_putstr_yx(std_plane, (int)box_y + 2, (int)box_x + 3, "[ EDIT ]");
    ncplane_set_fg_rgb(std_plane, 0xE3E0F7);
    ncplane_printf(std_plane, is_product ? " Modify product details"
                                         : " Rename project");

    int field_w = (int)box_w - 12;
    if (field_w < 4) field_w = 4;

    draw_input_field(std_plane, is_product ? "Product Name" : "Project Name",
                     edit->name_buf, field_w,
                     edit->active_field == FORM_FIELD_NAME,
                     (int)box_x, (int)box_y + 4);

    if (is_product) {
        draw_input_field(std_plane, "Local Path",
                         edit->path_buf, field_w,
                         edit->active_field == FORM_FIELD_PATH,
                         (int)box_x, (int)box_y + 7);
    }

    int br = (int)box_y + (int)box_h - 3;
    ncplane_set_fg_rgb(std_plane, 0x89B4FA);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 3, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 3, "│  [SAVE]  │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 3, "└──────────┘");
    ncplane_set_fg_rgb(std_plane, 0x585B70);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 49, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 49, "│ [CANCEL] │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 49, "└──────────┘");

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
