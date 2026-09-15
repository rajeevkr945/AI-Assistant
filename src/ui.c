#include <wchar.h>
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

// ─────────────────────────────────────────────────────────────────────────────
// Background animation (stub)
// ─────────────────────────────────────────────────────────────────────────────
void update_background_animation(struct ncplane* plane, uint64_t frame_tick) {
    (void)plane; (void)frame_tick;
}

// ─────────────────────────────────────────────────────────────────────────────
// Palette (Tokyo-Night-ish, but consistent with earlier version)
// ─────────────────────────────────────────────────────────────────────────────
#define COL_BG          0x121221
#define COL_BG_DIM      0x0E0E1A
#define COL_BG_HIGH     0x1E1E2E
#define COL_BORDER      0x585B70
#define COL_BORDER_HI   0x89B4FA
#define COL_TEXT        0xE3E0F7
#define COL_TEXT_DIM    0x888899
#define COL_TEXT_MUTED  0x556677
#define COL_ACCENT      0x00E478   // primary green
#define COL_ACCENT2     0x00E4C8   // teal
#define COL_WARN        0xFFDD00
#define COL_DANGER      0xFF4444
#define COL_LINK        0x89B4FA   // clickable link color
#define COL_PROJECT     0xCDB4FF

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
        ncplane_set_fg_rgb(p, COL_TEXT);
        ncplane_on_styles(p, NCSTYLE_BOLD);
        ncplane_printf_yx(p, 0, 2, " %s ", title);
        ncplane_off_styles(p, NCSTYLE_BOLD);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input field helper
// ─────────────────────────────────────────────────────────────────────────────
static void draw_input_field(struct ncplane* p,
                              const char* label,
                              const char* buf,
                              int field_w,
                              bool active,
                              int bx, int ry,
                              bool mask) {
    uint32_t bcol = active ? COL_ACCENT : COL_BORDER;
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
    char masked[MAX_PATH];
    const char* display = buf;
    if (mask) {
        size_t n = len < sizeof(masked)-1 ? len : sizeof(masked)-1;
        for (size_t i = 0; i < n; i++) masked[i] = '*';
        masked[n] = '\0';
        display = masked;
    }
    int visible = (int)len;
    if (visible > field_w) {
        display = display + (len - (size_t)field_w);
        visible = field_w;
    }
    ncplane_set_fg_rgb(p, active ? COL_ACCENT : COL_TEXT);
    ncplane_putstr_yx(p, ry + 1, bx + 7, display);

    if (active) {
        ncplane_set_bg_rgb(p, COL_ACCENT);
        ncplane_set_fg_rgb(p, COL_BG);
        ncplane_putchar_yx(p, ry + 1, bx + 7 + visible, ' ');
        ncplane_set_bg_rgb(p, COL_BG_HIGH);
        ncplane_set_fg_rgb(p, COL_ACCENT);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Disk usage (statvfs)
// ─────────────────────────────────────────────────────────────────────────────
static int read_disk_percent(const char* path, unsigned long* out_used_gb, unsigned long* out_total_gb) {
    if (!path || !*path) path = getenv("HOME");
    if (!path) path = ".";
    struct statvfs vfs;
    if (statvfs(path, &vfs) != 0) return -1;
    unsigned long block = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
    unsigned long total = (unsigned long)vfs.f_blocks * block;
    unsigned long avail = (unsigned long)vfs.f_bavail * block;
    if (total == 0) return -1;
    unsigned long used  = total - avail;
    *out_total_gb = total / (1024UL * 1024UL * 1024UL);
    *out_used_gb  = used  / (1024UL * 1024UL * 1024UL);
    return (int)((used * 100UL) / total);
}

// ─────────────────────────────────────────────────────────────────────────────
// Intro art
// ─────────────────────────────────────────────────────────────────────────────
static const char* SUBASH_ART[6][6] = {
    { " ████ ", "██    ", " ███  ", "   ██ ", "████  ", "      " },
    { "██  ██", "██  ██", "██  ██", "██  ██", " ████ ", "      " },
    { "█████ ", "██  ██", "█████ ", "██  ██", "█████ ", "      " },
    { " ████ ", "██  ██", "██████", "██  ██", "██  ██", "      " },
    { " ████ ", "██    ", " ███  ", "   ██ ", "████  ", "      " },
    { "██  ██", "██  ██", "██████", "██  ██", "██  ██", "      " },
};

static uint32_t intro_color(int frame, int col_offset, int row) {
    int t = (frame * 48 + col_offset * 7 + row * 11) % 360;
    if (t < 40)  return 0xFF3366;
    if (t < 80)  return 0xFF8800;
    if (t < 120) return 0xFFDD00;
    if (t < 160) return 0x00E478;
    if (t < 200) return 0x00E4C8;
    if (t < 240) return 0x00B4FF;
    if (t < 280) return 0x5588FF;
    if (t < 320) return 0xAA66FF;
    return 0xFF66CC;
}

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

    static const uint32_t edge_colors[] = {
        0x1A0A2E, 0x0A1A2E, 0x0A2E1A, 0x2E1A0A, 0x1A2E2E,
        0x2E0A1A, 0x0A0A2E, 0x2E2E0A, 0x1A1A2E, 0x0D0D1A
    };
    uint32_t bg = edge_colors[intro_frame % 10];
    ncplane_set_bg_rgb(left, bg);
    ncplane_set_fg_rgb(left, bg);
    for (unsigned int r = 0; r < full_rows; r++)
        for (unsigned int c = 0; c < full_cols; c++)
            ncplane_putchar_yx(left, (int)r, (int)c, ' ');

    const int art_w = 48, art_h = 6;
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
            uint32_t color = intro_color(intro_frame, li * 8 + row, row);
            ncplane_set_bg_rgb(left, bg);
            ncplane_set_fg_rgb(left, color);
            ncplane_on_styles(left, NCSTYLE_BOLD);
            ncplane_putstr_yx(left, ay + row, lx, line);
            ncplane_off_styles(left, NCSTYLE_BOLD);
        }
    }

    if (intro_frame >= 4) {
        int uy = ay + art_h;
        if (uy < (int)full_rows - 1)
            for (int c = 0; c < art_w && (ax + c) < (int)full_cols; c++) {
                ncplane_set_fg_rgb(left, intro_color(intro_frame, c, 0));
                ncplane_putstr_yx(left, uy, ax + c, "─");
            }
    }

    if (intro_frame >= 6) {
        ncplane_set_fg_rgb(left, 0xAADDFF);
        int sub_y = ay + art_h + 1;
        if (sub_y < (int)full_rows - 2)
            ncplane_putstr_yx(left, sub_y, ax, "  project & task organizer  ");
    }
    if (intro_frame >= 8) {
        ncplane_set_fg_rgb(left, intro_color(intro_frame, 20, 2));
        int tag_y = ay + art_h + 2;
        if (tag_y < (int)full_rows - 1)
            ncplane_putstr_yx(left, tag_y, ax, "  v2.0  ·  press ? for help  ");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Truncate/pad to fixed width
// ─────────────────────────────────────────────────────────────────────────────
static void fit_str(char* dest, size_t dest_sz, const char* src, int width) {
    if (dest_sz == 0 || width <= 0) return;
    size_t n = strlen(src);
    if ((int)n > width) n = (size_t)width;
    if (n >= dest_sz) n = dest_sz - 1;
    memcpy(dest, src, n);
    while ((int)n < width && n + 1 < dest_sz) dest[n++] = ' ';
    dest[n < dest_sz ? n : dest_sz - 1] = '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
// Git graph fetch (popen)
// ─────────────────────────────────────────────────────────────────────────────
void refresh_git_graph(AppState *state) {
    state->git_graph_cache[0] = '\0';
    state->git_log_scroll = 0; // Reset scroll offset on project switch
    state->git_graph_project_idx = state->selected_project;
    if (state->selected_project < 0 || state->selected_project >= state->project_count) return;

    const Project *proj = &state->projects[state->selected_project];
    if (!proj->local_git_path[0]) return;

    char expanded[MAX_PATH];
    expand_home(proj->local_git_path, expanded, sizeof(expanded));

    char cmd[MAX_PATH * 2 + 128];
    // Escape single quotes.
    char safe[MAX_PATH * 2];
    size_t si = 0;
    for (const char *p = expanded; *p && si < sizeof(safe) - 5; p++) {
        if (*p == '\'') { safe[si++]='\''; safe[si++]='\\'; safe[si++]='\''; safe[si++]='\''; }
        else safe[si++] = *p;
    }
    safe[si] = '\0';

    snprintf(cmd, sizeof(cmd),
        "git -C '%s' log --graph --all --date=relative --format='%%h|%%d|%%s|%%cr|%%an' -n 100 2>/dev/null",
        safe);

    FILE *fp = popen(cmd, "r");
    if (!fp) return;
    size_t total = 0;
    while (total < sizeof(state->git_graph_cache) - 1) {
        size_t got = fread(state->git_graph_cache + total, 1,
                           sizeof(state->git_graph_cache) - 1 - total, fp);
        if (got == 0) break;
        total += got;
    }
    state->git_graph_cache[total] = '\0';
    pclose(fp);
}

// Find commit hash within a graph-prefixed line.
// Returns a pointer to the start of the hash, and sets prefix_len and hash_len.
static const char *find_commit_hash(const char *line, int *prefix_len, int *hash_len) {
    int len = (int)strlen(line);
    for (int i = 0; i < len; i++) {
        int count = 0;
        while (i + count < len) {
            char c = line[i + count];
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                count++;
            } else {
                break;
            }
        }
        if (count >= 7 && count <= 16 && line[i + count] == '|') {
            if (prefix_len) *prefix_len = i;
            if (hash_len) *hash_len = count;
            return line + i;
        }
    }
    return NULL;
}

static const char *map_graph_char(char ch) {
    switch (ch) {
        case '*': return "●";
        case '|': return "│";
        case '/': return "╱";
        case '\\': return "╲";
        case '_': return "─";
        case '-': return "─";
        default: {
            static char buf[2] = {0};
            buf[0] = ch;
            return buf;
        }
    }
}

static uint32_t get_graph_color(char ch, int col_idx) {
    if (ch == '*') {
        return 0x00E478; // Bright green for commit node
    }
    static const uint32_t palette[] = {
        0x89B4FA, // Light Blue
        0x00E4C8, // Teal
        0xCDB4FF, // Soft Purple
        0xF470C0, // Pink
        0xFFDD00, // Amber Yellow
        0xA6E22E, // Lime Green
    };
    int num_colors = sizeof(palette) / sizeof(palette[0]);
    int branch_idx = col_idx / 2;
    return palette[branch_idx % num_colors];
}

static void draw_git_headers(struct ncplane* p, int y, int x, int max_w) {
    int graph_w = 12;
    int hash_len = 7;
    int hash_x = x + graph_w;
    int detail_start_x = hash_x + hash_len + 1;
    int detail_w = max_w - (graph_w + hash_len + 1);

    int date_w = 14;
    int author_w = 12;

    bool show_author = (detail_w > 40);
    bool show_date = (detail_w > 25);

    ncplane_set_fg_rgb(p, COL_BORDER_HI);
    ncplane_on_styles(p, NCSTYLE_UNDERLINE);

    ncplane_putstr_yx(p, y, x, "GRAPH");
    ncplane_putstr_yx(p, y, hash_x, "HASH");

    int commit_w = detail_w;
    if (show_author) commit_w -= (author_w + 2);
    if (show_date) commit_w -= (date_w + 2);

    if (commit_w > 14) {
        ncplane_putstr_yx(p, y, detail_start_x, "COMMIT MESSAGE");
    }

    if (show_author) {
        int author_x = x + max_w - (show_date ? (date_w + 2) : 0) - author_w;
        ncplane_putstr_yx(p, y, author_x, "AUTHOR");
    }

    if (show_date) {
        int date_x = x + max_w - date_w;
        ncplane_putstr_yx(p, y, date_x, "DATE");
    }

    ncplane_off_styles(p, NCSTYLE_UNDERLINE);
}

// VS Code like git-log line colorization and table layout
static void render_git_line(struct ncplane* p, int y, int x, int max_w, const char* line) {
    int prefix_len = 0;
    int hash_len = 0;
    const char *h_start = find_commit_hash(line, &prefix_len, &hash_len);

    if (!h_start) {
        // Pure graph line: render with branch colors
        int col = x;
        int written = 0;
        for (const char *c = line; *c && written < max_w; c++) {
            char ch = *c;
            if (ch == '\n' || ch == '\r') break;
            if (ch == ' ') {
                ncplane_putchar_yx(p, y, col, ' ');
            } else {
                ncplane_set_fg_rgb(p, get_graph_color(ch, written));
                ncplane_putstr_yx(p, y, col, map_graph_char(ch));
            }
            col++;
            written++;
        }
        return;
    }

    // Parse fields from commit line: prefix[hash]|[decorations]|[subject]|[relative_date]|[author]
    char hash[16] = {0};
    char decor[256] = {0};
    char subject[512] = {0};
    char date[128] = {0};
    char author[128] = {0};

    if (hash_len >= (int)sizeof(hash)) hash_len = sizeof(hash) - 1;
    memcpy(hash, h_start, hash_len);
    hash[hash_len] = '\0';

    const char *p1 = h_start + hash_len + 1; // skip hash and '|'
    const char *p2 = strchr(p1, '|');
    if (p2) {
        size_t len = p2 - p1;
        if (len >= sizeof(decor)) len = sizeof(decor) - 1;
        memcpy(decor, p1, len);
        decor[len] = '\0';

        const char *p3 = strchr(p2 + 1, '|');
        if (p3) {
            len = p3 - (p2 + 1);
            if (len >= sizeof(subject)) len = sizeof(subject) - 1;
            memcpy(subject, p2 + 1, len);
            subject[len] = '\0';

            const char *p4 = strchr(p3 + 1, '|');
            if (p4) {
                len = p4 - (p3 + 1);
                if (len >= sizeof(date)) len = sizeof(date) - 1;
                memcpy(date, p3 + 1, len);
                date[len] = '\0';

                // Author is everything after p4
                strncpy(author, p4 + 1, sizeof(author) - 1);
                author[sizeof(author) - 1] = '\0';
                size_t al = strlen(author);
                while (al > 0 && (author[al - 1] == '\n' || author[al - 1] == '\r')) {
                    author[al - 1] = '\0';
                    al--;
                }
            } else {
                strncpy(date, p3 + 1, sizeof(date) - 1);
            }
        } else {
            strncpy(subject, p2 + 1, sizeof(subject) - 1);
        }
    }

    // Trim decoration spaces
    char *trimmed_decor = decor;
    while (*trimmed_decor == ' ') trimmed_decor++;
    size_t dl = strlen(trimmed_decor);
    while (dl > 0 && trimmed_decor[dl - 1] == ' ') {
        trimmed_decor[dl - 1] = '\0';
        dl--;
    }

    // 1. Graph prefix (fixed width column)
    int col = x;
    int graph_w = 12; // fixed column width for graph
    for (int i = 0; i < prefix_len && i < graph_w; i++) {
        char ch = line[i];
        if (ch == ' ') {
            ncplane_putchar_yx(p, y, col + i, ' ');
        } else {
            ncplane_set_fg_rgb(p, get_graph_color(ch, i));
            ncplane_putstr_yx(p, y, col + i, map_graph_char(ch));
        }
    }
    // Fill remaining graph width with spaces
    for (int i = prefix_len; i < graph_w; i++) {
        ncplane_putchar_yx(p, y, col + i, ' ');
    }

    // 2. Commit Hash (orange)
    int hash_x = x + graph_w;
    ncplane_set_fg_rgb(p, 0xFF9F43);
    ncplane_putstr_yx(p, y, hash_x, hash);
    ncplane_putchar_yx(p, y, hash_x + hash_len, ' ');

    // Layout configuration
    int detail_start_x = hash_x + hash_len + 1;
    int detail_w = max_w - (graph_w + hash_len + 1);

    int date_w = 14;
    int author_w = 12;

    bool show_author = (detail_w > 40);
    bool show_date = (detail_w > 25);

    int subject_w = detail_w;
    if (show_author) subject_w -= (author_w + 2);
    if (show_date) subject_w -= (date_w + 2);

    // 3. Date column (right-aligned)
    if (show_date) {
        int date_x = x + max_w - date_w;
        char fitted_date[32];
        size_t dlen = strlen(date);
        if (dlen > (size_t)date_w) {
            strncpy(fitted_date, date, date_w);
            fitted_date[date_w] = '\0';
        } else {
            int pad = date_w - (int)dlen;
            for (int i = 0; i < pad; i++) fitted_date[i] = ' ';
            strcpy(fitted_date + pad, date);
        }
        ncplane_set_fg_rgb(p, COL_TEXT_DIM);
        ncplane_putstr_yx(p, y, date_x, fitted_date);
    }

    // 4. Author column
    if (show_author) {
        int author_x = x + max_w - (show_date ? (date_w + 2) : 0) - author_w;
        char fitted_author[32];
        size_t alen = strlen(author);
        if (alen > (size_t)author_w) {
            strncpy(fitted_author, author, author_w - 1);
            fitted_author[author_w - 1] = '\0';
            strcat(fitted_author, "~");
        } else {
            int pad = author_w - (int)alen;
            for (int i = 0; i < pad; i++) fitted_author[i] = ' ';
            strcpy(fitted_author + pad, author);
        }
        ncplane_set_fg_rgb(p, COL_PROJECT);
        ncplane_putstr_yx(p, y, author_x, fitted_author);
    }

    // 5. Subject and decorations
    int sub_x = detail_start_x;
    if (trimmed_decor[0] != '\0' && subject_w > 5) {
        // Draw decoration badge in blue/highlight border color
        ncplane_set_fg_rgb(p, COL_BORDER_HI);
        ncplane_putstr_yx(p, y, sub_x, trimmed_decor);
        ncplane_putchar(p, ' ');

        int decor_len = (int)strlen(trimmed_decor) + 1;
        sub_x += decor_len;
        subject_w -= decor_len;
    }

    if (subject_w > 0) {
        char fitted_sub[512];
        size_t slen = strlen(subject);
        if (slen > (size_t)subject_w) {
            if (subject_w > 3) {
                strncpy(fitted_sub, subject, subject_w - 3);
                fitted_sub[subject_w - 3] = '\0';
                strcat(fitted_sub, "...");
            } else {
                strncpy(fitted_sub, subject, subject_w);
                fitted_sub[subject_w] = '\0';
            }
        } else {
            strcpy(fitted_sub, subject);
        }
        ncplane_set_fg_rgb(p, COL_TEXT);
        ncplane_putstr_yx(p, y, sub_x, fitted_sub);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Path-exists check
// ─────────────────────────────────────────────────────────────────────────────
static bool path_exists(const char* path) {
    if (!path || !*path) return false;
    char expanded[MAX_PATH];
    expand_home(path, expanded, sizeof(expanded));
    return access(expanded, F_OK) == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw main interface (MODE_LIST)
// ─────────────────────────────────────────────────────────────────────────────
void draw_interface(struct notcurses* nc,
                    struct ncplane* left, struct ncplane* right,
                    AppState* state, uint64_t frame_tick,
                    const FilterState* filter,
                    HitMap* out_hit) {
    (void)frame_tick;
    if (out_hit) memset(out_hit, 0, sizeof(HitMap));

    struct ncplane* stdplane = notcurses_stdplane(nc);
    ncplane_erase(stdplane);

    unsigned int rows, cols;
    ncplane_dim_yx(stdplane, &rows, &cols);

    ncplane_set_bg_rgb(stdplane, COL_BG);
    for (unsigned int r = 0; r < rows; r++)
        for (unsigned int c = 0; c < cols; c++)
            ncplane_putchar_yx(stdplane, (int)r, (int)c, ' ');

    // top bar
    ncplane_set_fg_rgb(stdplane, COL_ACCENT);
    ncplane_on_styles(stdplane, NCSTYLE_BOLD);
    ncplane_putstr_yx(stdplane, 0, 2, "~SUBASH");
    ncplane_off_styles(stdplane, NCSTYLE_BOLD);
    if (state->nav_focus == NAV_FOCUS_PROJECT) {
        ncplane_set_fg_rgb(stdplane, COL_LINK);
        ncplane_putstr_yx(stdplane, 0, 12, "[ PROJECTS  ◀ ]");
    } else {
        ncplane_set_fg_rgb(stdplane, COL_ACCENT);
        ncplane_putstr_yx(stdplane, 0, 12, "[ TASKS  ▶ ]");
    }
    ncplane_set_fg_rgb(stdplane, COL_TEXT_DIM);
    ncplane_printf_yx(stdplane, 0, 32, " %d projects · %d tasks ",
                      state->project_count, get_total_product_count(state));
    ncplane_set_fg_rgb(stdplane, COL_BORDER);
    for (unsigned int c = 0; c < cols; c++)
        ncplane_putstr_yx(stdplane, 1, (int)c, "─");

    // footer
    int footer_y = (int)rows - 1;
    ncplane_set_fg_rgb(stdplane, COL_BORDER);
    for (unsigned int c = 0; c < cols; c++)
        ncplane_putstr_yx(stdplane, footer_y - 1, (int)c, "─");

    ncplane_set_fg_rgb(stdplane, COL_TEXT_DIM);
    ncplane_putstr_yx(stdplane, footer_y, 2, "");
    #define HINT(k,label,col) do { \
        ncplane_set_fg_rgb(stdplane, (col)); ncplane_putstr(stdplane, (k)); \
        ncplane_set_fg_rgb(stdplane, COL_TEXT); ncplane_putstr(stdplane, (label)); \
    } while (0)
    HINT("[a]", " Add Task ",  COL_ACCENT);
    HINT("[n]", " New Proj ",  COL_ACCENT);
    HINT("[e]", " Edit ",      COL_ACCENT);
    HINT("[d]", " Del Task ",  COL_DANGER);
    HINT("[D]", " Del Proj ",  COL_DANGER);
    HINT("[o]", " Obsidian ",  COL_LINK);
    HINT("[g]", " Gerrit ",    COL_LINK);
    HINT("[r]", " Redmine ",   COL_LINK);
    HINT("[/]", " Filter ",    COL_ACCENT);
    HINT("[?]", " Help ",      COL_ACCENT);
    HINT("[q]", " Quit",       COL_ACCENT);
    #undef HINT

    // ── Empty state ─────────────────────────────────────────────────────────
    if (state->project_count == 0) {
        ncplane_erase(left);
        ncplane_erase(right);
        int cy = (int)rows / 2 - 2;
        int cx = (int)cols / 2 - 22;
        if (cx < 2) cx = 2;
        ncplane_set_fg_rgb(stdplane, COL_ACCENT);
        ncplane_on_styles(stdplane, NCSTYLE_BOLD);
        ncplane_putstr_yx(stdplane, cy,     cx, "  no projects yet.");
        ncplane_off_styles(stdplane, NCSTYLE_BOLD);
        ncplane_set_fg_rgb(stdplane, COL_TEXT);
        ncplane_putstr_yx(stdplane, cy + 2, cx, "  Press [n] to create your first project.");
        ncplane_set_fg_rgb(stdplane, COL_TEXT_DIM);
        ncplane_putstr_yx(stdplane, cy + 3, cx, "  Press [?] to open the manual.");
        return;
    }

    // ── Left pane (project list) ────────────────────────────────────────────
    ncplane_erase(left);
    ncplane_set_bg_rgb(left, COL_BG);
    unsigned int l_rows, l_cols;
    ncplane_dim_yx(left, &l_rows, &l_cols);

    bool proj_focus = (state->nav_focus == NAV_FOCUS_PROJECT);
    draw_panel_border(left, proj_focus ? COL_BORDER_HI : COL_BORDER, "PROJECTS");

    int max_proj_rows = (int)l_rows - 3;
    if (max_proj_rows < 1) max_proj_rows = 1;
    int start_pi = 0;
    if (state->project_count > max_proj_rows) {
        start_pi = state->selected_project - max_proj_rows / 2;
        if (start_pi < 0) start_pi = 0;
        if (start_pi + max_proj_rows > state->project_count)
            start_pi = state->project_count - max_proj_rows;
    }

    int row = 2;
    for (int i = start_pi; i < state->project_count && (row - 1) < max_proj_rows + 1; i++) {
        const Project* proj = &state->projects[i];
        bool is_active = (i == state->selected_project);

        int name_w = (int)l_cols - 8;
        if (name_w < 1) name_w = 1;
        char nbuf[64];
        fit_str(nbuf, sizeof(nbuf), proj->name, name_w > 60 ? 60 : name_w);

        if (is_active) {
            ncplane_set_fg_rgb(left, proj_focus ? COL_ACCENT : COL_LINK);
            ncplane_putstr_yx(left, row, 1, proj_focus ? "❯" : "·");
            ncplane_set_fg_rgb(left, COL_LINK);
            ncplane_on_styles(left, NCSTYLE_BOLD);
            ncplane_printf_yx(left, row, 3, "%s", nbuf);
            ncplane_off_styles(left, NCSTYLE_BOLD);
        } else {
            ncplane_set_fg_rgb(left, COL_TEXT_MUTED);
            ncplane_putstr_yx(left, row, 1, " ");
            ncplane_printf_yx(left, row, 3, "%s", nbuf);
        }
        // count badge (right-aligned)
        char cnt[8];
        snprintf(cnt, sizeof(cnt), "%d", proj->product_count);
        int cnt_x = (int)l_cols - 2 - (int)strlen(cnt);
        if (cnt_x > 3) {
            ncplane_set_fg_rgb(left, is_active ? COL_ACCENT : COL_TEXT_DIM);
            ncplane_putstr_yx(left, row, cnt_x, cnt);
        }
        row++;
    }

    // ── Right pane split ────────────────────────────────────────────────────
    ncplane_erase(right);
    ncplane_set_bg_rgb(right, COL_BG);
    unsigned int r_rows, r_cols;
    ncplane_dim_yx(right, &r_rows, &r_cols);

    bool prod_focus = (state->nav_focus == NAV_FOCUS_PRODUCT);
    draw_panel_border(right, prod_focus ? COL_ACCENT : COL_BORDER, "");

    // Sub-layout:
    // Rows 0..detail_h-1  : project details card (5 lines)
    // Rows detail_h..     : tasks table
    // Bottom 8 lines      : git graph pane (if git_path set)
    const Project *proj = &state->projects[state->selected_project];

    int detail_h = 6;
    int graph_h  = (proj->local_git_path[0]) ? 12 : 0;
    if ((int)r_rows < detail_h + graph_h + 6) graph_h = 0;
    if ((int)r_rows < detail_h + 4) detail_h = 4;
    int table_top = detail_h;
    int table_bottom = (int)r_rows - graph_h - 2;

    // ── Project details card ───────────────────────────────────────────────
    ncplane_set_fg_rgb(right, COL_TEXT);
    ncplane_on_styles(right, NCSTYLE_BOLD);
    ncplane_putstr_yx(right, 0, 2, "┌─ PROJECT ");
    ncplane_off_styles(right, NCSTYLE_BOLD);
    ncplane_set_fg_rgb(right, COL_LINK);
    ncplane_printf(right, "%s ", proj->name);
    ncplane_set_fg_rgb(right, COL_TEXT_DIM);
    ncplane_printf(right, " %d tasks ", proj->product_count);

    int dy = 1;
    ncplane_set_fg_rgb(right, COL_TEXT_MUTED);
    if (proj->description[0]) {
        char desc[MAX_DESC];
        fit_str(desc, sizeof(desc), proj->description, (int)r_cols - 6);
        ncplane_set_fg_rgb(right, COL_TEXT);
        ncplane_printf_yx(right, dy++, 3, "%s", desc);
    } else {
        ncplane_set_fg_rgb(right, COL_TEXT_MUTED);
        ncplane_printf_yx(right, dy++, 3, "(no description — press [P] to add)");
    }

    ncplane_set_fg_rgb(right, COL_TEXT_DIM);
    ncplane_printf_yx(right, dy++, 3, "git:     ");
    ncplane_set_fg_rgb(right, proj->local_git_path[0] ? COL_TEXT : COL_TEXT_MUTED);
    ncplane_printf(right, "%s", proj->local_git_path[0] ? proj->local_git_path : "—");

    ncplane_set_fg_rgb(right, COL_TEXT_DIM);
    ncplane_printf_yx(right, dy++, 3, "gerrit:  ");
    ncplane_set_fg_rgb(right, proj->gerrit_url[0] ? COL_LINK : COL_TEXT_MUTED);
    ncplane_printf(right, "%s", proj->gerrit_url[0] ? proj->gerrit_url : "—");

    ncplane_set_fg_rgb(right, COL_TEXT_DIM);
    ncplane_printf_yx(right, dy++, 3, "redmine: ");
    ncplane_set_fg_rgb(right, proj->redmine_url[0] ? COL_LINK : COL_TEXT_MUTED);
    ncplane_printf(right, "%s", proj->redmine_url[0] ? proj->redmine_url : "—");

    // divider
    ncplane_set_fg_rgb(right, COL_BORDER);
    for (unsigned int c = 2; c < r_cols - 1; c++)
        ncplane_putstr_yx(right, table_top - 1, (int)c, "─");

    // ── Tasks table ────────────────────────────────────────────────────────
    ncplane_set_fg_rgb(right, COL_TEXT);
    ncplane_on_styles(right, NCSTYLE_BOLD);
    ncplane_putstr_yx(right, table_top, 2, "TASKS");
    ncplane_off_styles(right, NCSTYLE_BOLD);

    if (filter && filter->query[0]) {
        ncplane_set_fg_rgb(right, COL_WARN);
        ncplane_printf(right, "   filter: /%s", filter->query);
    }

    const int COL_STATE = 2;
    const int COL_ID    = 4;
    const int COL_NAME  = 9;
    const int NAME_W    = 28;
    const int COL_META  = COL_NAME + NAME_W + 2;
    const int COL_PATH  = COL_META + 10;

    int header_y = table_top + 1;
    ncplane_set_fg_rgb(right, COL_TEXT_DIM);
    ncplane_on_styles(right, NCSTYLE_BOLD);
    ncplane_printf_yx(right, header_y, COL_ID,   "%-4s", "ID");
    ncplane_printf_yx(right, header_y, COL_NAME, "%-*s", NAME_W, "TASK");
    ncplane_printf_yx(right, header_y, COL_META, "%-8s", "STATUS");
    if (COL_PATH < (int)r_cols - 2)
        ncplane_printf_yx(right, header_y, COL_PATH, "%s", "PATH / DOC");
    ncplane_off_styles(right, NCSTYLE_BOLD);
    ncplane_set_fg_rgb(right, COL_BORDER);
    for (unsigned int c = 2; c < r_cols - 1; c++)
        ncplane_putstr_yx(right, header_y + 1, (int)c, "─");

    int py = header_y + 2;
    int table_max = table_bottom - py;
    if (table_max < 1) table_max = 1;

    int py_start = py;
    for (int j = 0; j < proj->product_count; j++) {
        if (py - py_start >= table_max) break;
        const Product* prod = &proj->products[j];

        if (filter && filter->query[0] &&
            !fuzzy_match(filter->query, prod->name) &&
            !fuzzy_match(filter->query, prod->local_path)) {
            continue;
        }

        bool selected = (j == state->selected_product) && prod_focus;
        bool marked   = (j == state->selected_product);
        bool exists   = path_exists(prod->local_path);
        bool has_remote = prod->remote_host[0] != '\0';

        if (selected) {
            ncplane_set_fg_rgb(right, COL_ACCENT);
            ncplane_putstr_yx(right, py, 1, "❯");
            ncplane_on_styles(right, NCSTYLE_BOLD);
        } else if (marked) {
            ncplane_set_fg_rgb(right, COL_LINK);
            ncplane_putstr_yx(right, py, 1, "·");
        } else {
            ncplane_putstr_yx(right, py, 1, " ");
        }

        // state dot
        ncplane_set_fg_rgb(right, exists ? COL_ACCENT : (has_remote ? COL_WARN : COL_TEXT_MUTED));
        ncplane_putstr_yx(right, py, COL_STATE, exists ? "●" : "○");

        ncplane_set_fg_rgb(right, COL_TEXT_DIM);
        ncplane_printf_yx(right, py, COL_ID, "%03d", j + 1);

        char name_disp[64];
        fit_str(name_disp, sizeof(name_disp), prod->name, NAME_W);
        ncplane_set_fg_rgb(right, selected ? COL_LINK : (marked ? COL_PROJECT : COL_TEXT));
        ncplane_printf_yx(right, py, COL_NAME, "%s", name_disp);

        if (exists) {
            ncplane_set_fg_rgb(right, COL_ACCENT);
            ncplane_printf_yx(right, py, COL_META, "[ READY ]");
        } else if (has_remote) {
            ncplane_set_fg_rgb(right, COL_WARN);
            ncplane_printf_yx(right, py, COL_META, "[REMOTE ]");
        } else {
            ncplane_set_fg_rgb(right, COL_TEXT_MUTED);
            ncplane_printf_yx(right, py, COL_META, "[ MISS  ]");
        }

        // Path column: show obsidian if present (clickable indicator), else path.
        int path_w = (int)r_cols - COL_PATH - 2;
        if (path_w > 0 && COL_PATH < (int)r_cols - 2) {
            const char* dp = "";
            bool is_obsidian = false;
            if (prod->obsidian_path[0]) { dp = prod->obsidian_path; is_obsidian = true; }
            else if (prod->local_path[0]) dp = prod->local_path;
            else if (prod->notes[0])      dp = prod->notes;

            size_t pln = strlen(dp);
            char shown[MAX_PATH];
            if ((int)pln > path_w && path_w > 3) {
                snprintf(shown, sizeof(shown), "..%s", dp + (pln - (size_t)path_w + 2));
            } else {
                snprintf(shown, sizeof(shown), "%s", dp);
            }
            ncplane_set_fg_rgb(right, is_obsidian ? COL_LINK : COL_TEXT_MUTED);
            if (is_obsidian) ncplane_on_styles(right, NCSTYLE_UNDERLINE);
            ncplane_printf_yx(right, py, COL_PATH, "%s", shown);
            if (is_obsidian) ncplane_off_styles(right, NCSTYLE_UNDERLINE);

            // record hit target for selected row
            if (selected && is_obsidian && out_hit) {
                // Left/right panes are offset from stdplane; caller adjusts.
                out_hit->obsidian_y = py;
                out_hit->obsidian_x = COL_PATH;
                out_hit->obsidian_w = (int)strlen(shown);
                strncpy(out_hit->obsidian_target, prod->obsidian_path,
                        sizeof(out_hit->obsidian_target) - 1);
            }
        }

        if (selected) ncplane_off_styles(right, NCSTYLE_BOLD);
        py++;
    }

    // ── Git graph pane ─────────────────────────────────────────────────────
    if (graph_h > 0) {
        int gy = (int)r_rows - graph_h - 1;
        
        // Horizontal separator line
        ncplane_set_fg_rgb(right, COL_BORDER);
        for (unsigned int c = 2; c < r_cols - 1; c++)
            ncplane_putstr_yx(right, gy - 1, (int)c, "─");

        // Focus indicator and Title
        bool git_focused = (state->nav_focus == NAV_FOCUS_GIT_LOG);
        ncplane_set_fg_rgb(right, git_focused ? COL_ACCENT : COL_ACCENT2);
        ncplane_on_styles(right, NCSTYLE_BOLD);
        ncplane_putstr_yx(right, gy, 2, git_focused ? "▶ GIT LOG" : "  GIT LOG");
        ncplane_off_styles(right, NCSTYLE_BOLD);
        ncplane_set_fg_rgb(right, COL_TEXT_DIM);
        ncplane_printf(right, "   %s", proj->local_git_path);

        // Column Headers
        draw_git_headers(right, gy + 1, 2, (int)r_cols - 4);
        
        // Header Divider
        ncplane_set_fg_rgb(right, COL_BORDER);
        for (unsigned int c = 2; c < r_cols - 2; c++)
            ncplane_putstr_yx(right, gy + 2, (int)c, "─");

        // Render Commits
        int gline_y = gy + 3;
        int gline_max = graph_h - 3;
        if (gline_max < 0) gline_max = 0;
        int rendered = 0;

        // Skip state->git_log_scroll lines
        int skipped = 0;
        const char *p = state->git_graph_cache;
        while (*p && skipped < state->git_log_scroll) {
            const char *end = strchr(p, '\n');
            if (!end) {
                p = "";
                break;
            }
            p = end + 1;
            skipped++;
        }

        // Draw commits
        while (*p && rendered < gline_max) {
            const char *end = strchr(p, '\n');
            size_t len = end ? (size_t)(end - p) : strlen(p);
            char buf[512];
            if (len >= sizeof(buf)) len = sizeof(buf) - 1;
            memcpy(buf, p, len);
            buf[len] = '\0';
            render_git_line(right, gline_y + rendered, 2, (int)r_cols - 4, buf);
            rendered++;
            if (!end) break;
            p = end + 1;
        }

        // Draw scrollbar if there are more commits than visible space
        int total_lines = 0;
        const char *tc = state->git_graph_cache;
        while (*tc) {
            const char *end = strchr(tc, '\n');
            total_lines++;
            if (!end) break;
            tc = end + 1;
        }

        if (total_lines > gline_max && gline_max > 0) {
            int sb_x = (int)r_cols - 2;
            // Draw scroll track
            ncplane_set_fg_rgb(right, COL_BORDER);
            for (int i = 0; i < gline_max; i++) {
                ncplane_putstr_yx(right, gline_y + i, sb_x, "│");
            }
            // Draw thumb
            int thumb_h = (gline_max * gline_max + total_lines - 1) / total_lines;
            if (thumb_h < 1) thumb_h = 1;
            if (thumb_h > gline_max) thumb_h = gline_max;
            
            int max_scroll = total_lines - gline_max;
            int thumb_y = 0;
            if (max_scroll > 0) {
                thumb_y = (state->git_log_scroll * (gline_max - thumb_h) + max_scroll - 1) / max_scroll;
            }
            if (thumb_y < 0) thumb_y = 0;
            if (thumb_y + thumb_h > gline_max) thumb_y = gline_max - thumb_h;

            ncplane_set_fg_rgb(right, COL_BORDER_HI);
            for (int i = 0; i < thumb_h; i++) {
                ncplane_putstr_yx(right, gline_y + thumb_y + i, sb_x, "█");
            }
        }

        if (rendered == 0 && state->git_log_scroll == 0) {
            ncplane_set_fg_rgb(right, COL_TEXT_MUTED);
            ncplane_putstr_yx(right, gline_y, 2, "(no git log — is git_path a git repo?)");
        }
    }

    // ── Bottom stats bar ────────────────────────────────────────────────────
    int stats_y = (int)r_rows - 1;
    // Disk stats path: project git_path if set, else HOME
    char statpath[MAX_PATH];
    if (proj->local_git_path[0])
        expand_home(proj->local_git_path, statpath, sizeof(statpath));
    else
        expand_home("~", statpath, sizeof(statpath));

    unsigned long used_gb = 0, total_gb = 0;
    int disk_pct = read_disk_percent(statpath, &used_gb, &total_gb);

    ncplane_set_fg_rgb(right, COL_TEXT_DIM);
    ncplane_putstr_yx(right, stats_y, 2, "DISK: ");
    if (disk_pct >= 0) {
        int filled = (disk_pct * 10 + 50) / 100;
        if (filled > 10) filled = 10;
        uint32_t bar_color = (disk_pct < 60) ? COL_LINK :
                             (disk_pct < 85) ? COL_WARN : COL_DANGER;
        ncplane_set_fg_rgb(right, bar_color);
        for (int b = 0; b < 10; b++) ncplane_putstr(right, b < filled ? "█" : "░");
        ncplane_set_fg_rgb(right, COL_TEXT_DIM);
        ncplane_printf(right, " %d%%  %luGB/%luGB", disk_pct, used_gb, total_gb);
    } else {
        ncplane_set_fg_rgb(right, COL_BORDER);
        ncplane_putstr(right, "unavailable");
    }

    // Peek at selected task's redmine/gerrit hint
    Product* sel = get_selected_product(state, NULL);
    if (sel && (int)r_cols > 60) {
        int peek_x = (int)r_cols / 2;
        ncplane_set_fg_rgb(right, COL_TEXT_MUTED);
        if (sel->redmine[0]) {
            char tbuf[64]; fit_str(tbuf, sizeof(tbuf), sel->redmine, 44);
            ncplane_printf_yx(right, stats_y, peek_x, "redmine: %s", tbuf);
        } else if (sel->gerrit_commit[0]) {
            char tbuf[64]; fit_str(tbuf, sizeof(tbuf), sel->gerrit_commit, 44);
            ncplane_printf_yx(right, stats_y, peek_x, "gerrit: %s", tbuf);
        } else if (sel->remote_host[0]) {
            char tbuf[64]; fit_str(tbuf, sizeof(tbuf), sel->remote_host, 44);
            ncplane_printf_yx(right, stats_y, peek_x, "remote: %s", tbuf);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Form background + footer
// ─────────────────────────────────────────────────────────────────────────────
static void fill_form_bg(struct ncplane* p) {
    unsigned int rows, cols;
    ncplane_dim_yx(p, &rows, &cols);
    ncplane_set_bg_rgb(p, COL_BG);
    for (unsigned int r = 0; r < rows; r++)
        for (unsigned int c = 0; c < cols; c++)
            ncplane_putchar_yx(p, (int)r, (int)c, ' ');
    ncplane_set_fg_rgb(p, COL_ACCENT);
    ncplane_on_styles(p, NCSTYLE_BOLD);
    ncplane_putstr_yx(p, 0, 2, "⚡ SUBASH");
    ncplane_off_styles(p, NCSTYLE_BOLD);
    ncplane_set_fg_rgb(p, COL_BORDER);
    for (unsigned int c = 0; c < cols; c++)
        ncplane_putstr_yx(p, 1, (int)c, "─");
}

static void draw_form_footer(struct ncplane* p, const char* hints) {
    unsigned int rows, cols;
    ncplane_dim_yx(p, &rows, &cols);
    int fy = (int)rows - 1;
    ncplane_set_fg_rgb(p, COL_BORDER);
    for (unsigned int c = 0; c < cols; c++)
        ncplane_putstr_yx(p, fy - 1, (int)c, "─");
    ncplane_set_fg_rgb(p, COL_TEXT_DIM);
    ncplane_putstr_yx(p, fy, 2, hints);
}

// ─────────────────────────────────────────────────────────────────────────────
// Full-screen ADD form
// ─────────────────────────────────────────────────────────────────────────────
void draw_form(struct notcurses* nc, struct ncplane* std_plane,
               const FormState* form, const AppState* state) {
    (void)nc;
    fill_form_bg(std_plane);

    unsigned int rows, cols;
    ncplane_dim_yx(std_plane, &rows, &cols);

    bool is_task = (form->form_mode == FORM_MODE_ADD_PRODUCT);
    unsigned int box_y = 3;
    unsigned int box_x = 2;
    unsigned int box_w = (cols > 4) ? cols - 4 : cols;
    unsigned int box_h = (rows > 6) ? rows - 6 : rows;

    ncplane_set_bg_rgb(std_plane, COL_BG_HIGH);
    for (unsigned int r = box_y; r < box_y + box_h; r++)
        for (unsigned int c = box_x; c < box_x + box_w; c++)
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');

    // border on the box (redraw the outer border with title)
    ncplane_set_fg_rgb(std_plane, COL_ACCENT);
    ncplane_putstr_yx(std_plane, (int)box_y, (int)box_x, "┌");
    for (unsigned int c = box_x + 1; c < box_x + box_w - 1; c++)
        ncplane_putstr_yx(std_plane, (int)box_y, (int)c, "─");
    ncplane_putstr_yx(std_plane, (int)box_y, (int)(box_x + box_w - 1), "┐");
    for (unsigned int r = box_y + 1; r < box_y + box_h - 1; r++) {
        ncplane_putstr_yx(std_plane, (int)r, (int)box_x, "│");
        ncplane_putstr_yx(std_plane, (int)r, (int)(box_x + box_w - 1), "│");
    }
    ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)box_x, "└");
    for (unsigned int c = box_x + 1; c < box_x + box_w - 1; c++)
        ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)c, "─");
    ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)(box_x + box_w - 1), "┘");

    ncplane_set_fg_rgb(std_plane, COL_TEXT);
    ncplane_on_styles(std_plane, NCSTYLE_BOLD);
    ncplane_printf_yx(std_plane, (int)box_y, (int)box_x + 3, " %s ",
                      is_task ? "ADD TASK" : "NEW PROJECT");
    ncplane_off_styles(std_plane, NCSTYLE_BOLD);

    ncplane_set_fg_rgb(std_plane, COL_TEXT);
    ncplane_printf_yx(std_plane, (int)box_y + 1, (int)box_x + 3,
        is_task ? "  Fill fields — Tab/↑↓ to move, Enter to save."
                : "  Create a new project workspace.");

    int col_w = ((int)box_w - 8) / 2;
    if (col_w < 20) col_w = (int)box_w - 8;
    int col_field_w = col_w - 6;

    if (is_task) {
        int lx = (int)box_x + 2;
        int rx = (int)box_x + 4 + col_w;
        int y  = (int)box_y + 3;

        draw_input_field(std_plane, "Project Name", form->new_project, col_field_w,
                         form->active_field == FORM_FIELD_PROJECT, lx, y, false);
        draw_input_field(std_plane, "Remote Host", form->new_remote_host, col_field_w,
                         form->active_field == FORM_FIELD_REMOTE_HOST, rx, y, false);
        y += 3;
        draw_input_field(std_plane, "Task Name", form->new_name, col_field_w,
                         form->active_field == FORM_FIELD_NAME, lx, y, false);
        draw_input_field(std_plane, "Remote User", form->new_remote_user, col_field_w,
                         form->active_field == FORM_FIELD_REMOTE_USER, rx, y, false);
        y += 3;
        draw_input_field(std_plane, "Local Path", form->new_path, col_field_w,
                         form->active_field == FORM_FIELD_PATH, lx, y, false);
        draw_input_field(std_plane, "Remote Password", form->new_remote_pass, col_field_w,
                         form->active_field == FORM_FIELD_REMOTE_PASS, rx, y, true);
        y += 3;
        draw_input_field(std_plane, "Obsidian Doc (.md)", form->new_obsidian, col_field_w,
                         form->active_field == FORM_FIELD_OBSIDIAN, lx, y, false);
        draw_input_field(std_plane, "Gerrit Commit", form->new_gerrit_commit, col_field_w,
                         form->active_field == FORM_FIELD_GERRIT_COMMIT, rx, y, false);
        y += 3;
        draw_input_field(std_plane, "Redmine Link", form->new_redmine, col_field_w,
                         form->active_field == FORM_FIELD_REDMINE, lx, y, false);
        draw_input_field(std_plane, "Notes", form->new_notes, col_field_w,
                         form->active_field == FORM_FIELD_NOTES, rx, y, false);

        // existing project list hint
        int hint_y = y + 4;
        if (hint_y < (int)(box_y + box_h - 3) && state->project_count > 0) {
            ncplane_set_fg_rgb(std_plane, COL_TEXT_MUTED);
            ncplane_putstr_yx(std_plane, hint_y, lx + 3, "existing projects:");
            int hy = hint_y + 1, hx = lx + 5;
            for (int i = 0; i < state->project_count && i < 12; i++) {
                bool match = (strcmp(state->projects[i].name, form->new_project) == 0);
                ncplane_set_fg_rgb(std_plane, match ? COL_ACCENT : COL_TEXT_DIM);
                ncplane_printf_yx(std_plane, hy, hx, "· %s  ", state->projects[i].name);
                hx += (int)strlen(state->projects[i].name) + 5;
                if (hx > (int)box_x + (int)box_w - 20) { hy++; hx = lx + 5; }
            }
        }
    } else {
        // New Project — 5 fields, single column
        int lx = (int)box_x + 2;
        int field_w = (int)box_w - 10;
        int y = (int)box_y + 3;
        draw_input_field(std_plane, "Project Name", form->new_name, field_w,
                         form->active_field == FORM_FIELD_NAME, lx, y, false); y += 3;
        draw_input_field(std_plane, "Description", form->new_description, field_w,
                         form->active_field == FORM_FIELD_PROJ_DESC, lx, y, false); y += 3;
        draw_input_field(std_plane, "Local Git Path", form->new_git_path, field_w,
                         form->active_field == FORM_FIELD_PROJ_GIT_PATH, lx, y, false); y += 3;
        draw_input_field(std_plane, "Gerrit URL", form->new_gerrit_url, field_w,
                         form->active_field == FORM_FIELD_PROJ_GERRIT_URL, lx, y, false); y += 3;
        draw_input_field(std_plane, "Redmine URL", form->new_redmine_url, field_w,
                         form->active_field == FORM_FIELD_PROJ_REDMINE_URL, lx, y, false);
    }

    // Buttons
    int br = (int)(box_y + box_h - 4);
    ncplane_set_fg_rgb(std_plane, COL_ACCENT);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 3, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 3, "│  [SAVE]  │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 3, "└──────────┘");
    int cancel_x = (int)box_x + (int)box_w - 15;
    ncplane_set_fg_rgb(std_plane, COL_BORDER);
    ncplane_putstr_yx(std_plane, br,     cancel_x, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, cancel_x, "│ [CANCEL] │");
    ncplane_putstr_yx(std_plane, br + 2, cancel_x, "└──────────┘");

    if (form->error_msg[0] != '\0') {
        ncplane_set_fg_rgb(std_plane, COL_DANGER);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf_yx(std_plane, br - 1, (int)box_x + 3, "! %s", form->error_msg);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
    }

    draw_form_footer(std_plane,
        is_task ? " [Tab/↑↓] Field   [Enter] Next/Save   [Esc] Cancel"
                : " [Tab/↑↓] Field   [Enter] Save   [Esc] Cancel");
}

// ─────────────────────────────────────────────────────────────────────────────
// Confirm dialog
// ─────────────────────────────────────────────────────────────────────────────
void draw_confirm(struct notcurses* nc, struct ncplane* std_plane,
                  const ConfirmState* confirm, const AppState* state) {
    (void)nc; (void)state;
    fill_form_bg(std_plane);

    unsigned int rows, cols;
    ncplane_dim_yx(std_plane, &rows, &cols);

    unsigned int box_h = 9, box_w = 62;
    if (box_w > cols) box_w = cols;
    if (box_h > rows) box_h = rows;
    unsigned int box_y = (rows > box_h) ? (rows - box_h) / 2 : 0;
    unsigned int box_x = (cols > box_w) ? (cols - box_w) / 2 : 0;

    ncplane_set_bg_rgb(std_plane, COL_BG_HIGH);
    for (unsigned int r = box_y; r < box_y + box_h; r++)
        for (unsigned int c = box_x; c < box_x + box_w; c++)
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');

    ncplane_set_fg_rgb(std_plane, COL_DANGER);
    ncplane_putstr_yx(std_plane, (int)box_y, (int)box_x, "┌");
    for (unsigned int c = box_x + 1; c < box_x + box_w - 1; c++)
        ncplane_putstr_yx(std_plane, (int)box_y, (int)c, "─");
    ncplane_putstr_yx(std_plane, (int)box_y, (int)(box_x + box_w - 1), "┐");
    for (unsigned int r = box_y + 1; r < box_y + box_h - 1; r++) {
        ncplane_putstr_yx(std_plane, (int)r, (int)box_x, "│");
        ncplane_putstr_yx(std_plane, (int)r, (int)(box_x + box_w - 1), "│");
    }
    ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)box_x, "└");
    for (unsigned int c = box_x + 1; c < box_x + box_w - 1; c++)
        ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)c, "─");
    ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)(box_x + box_w - 1), "┘");

    ncplane_set_fg_rgb(std_plane, 0xFF6B6B);
    ncplane_on_styles(std_plane, NCSTYLE_BOLD);
    ncplane_putstr_yx(std_plane, (int)box_y + 1, (int)box_x + 3, " CONFIRM DELETE ");
    ncplane_putstr_yx(std_plane, (int)box_y + 2, (int)box_x + 3, "⚠  This action cannot be undone.");
    ncplane_off_styles(std_plane, NCSTYLE_BOLD);

    ncplane_set_fg_rgb(std_plane, COL_TEXT);
    if (confirm->action == CONFIRM_DELETE_PRODUCT) {
        ncplane_printf_yx(std_plane, (int)box_y + 3, (int)box_x + 3, "Delete task: ");
        ncplane_set_fg_rgb(std_plane, COL_LINK);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf(std_plane, "%s", confirm->subject_name);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
    } else {
        ncplane_printf_yx(std_plane, (int)box_y + 3, (int)box_x + 3, "Delete project: ");
        ncplane_set_fg_rgb(std_plane, COL_DANGER);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf(std_plane, "%s", confirm->subject_name);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
        ncplane_set_fg_rgb(std_plane, COL_TEXT);
        ncplane_printf_yx(std_plane, (int)box_y + 4, (int)box_x + 3,
                          "(all tasks in this project will also be removed)");
    }

    int br = (int)(box_y + box_h - 3);
    ncplane_set_fg_rgb(std_plane, COL_DANGER);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 3,  "┌────────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 3,  "│  [y] YES   │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 3,  "└────────────┘");
    ncplane_set_fg_rgb(std_plane, COL_ACCENT);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 43, "┌────────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 43, "│  [n] NO    │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 43, "└────────────┘");

    draw_form_footer(std_plane, " [y] Confirm Delete   [n / Esc] Cancel");
}

// ─────────────────────────────────────────────────────────────────────────────
// Full-screen EDIT form (mirror of ADD)
// ─────────────────────────────────────────────────────────────────────────────
void draw_edit(struct notcurses* nc, struct ncplane* std_plane,
               const EditState* edit, const AppState* state) {
    (void)nc; (void)state;
    fill_form_bg(std_plane);

    unsigned int rows, cols;
    ncplane_dim_yx(std_plane, &rows, &cols);

    bool is_task = (edit->target == EDIT_TARGET_PRODUCT);
    unsigned int box_y = 3;
    unsigned int box_x = 2;
    unsigned int box_w = (cols > 4) ? cols - 4 : cols;
    unsigned int box_h = (rows > 6) ? rows - 6 : rows;

    ncplane_set_bg_rgb(std_plane, COL_BG_HIGH);
    for (unsigned int r = box_y; r < box_y + box_h; r++)
        for (unsigned int c = box_x; c < box_x + box_w; c++)
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');

    ncplane_set_fg_rgb(std_plane, COL_LINK);
    ncplane_putstr_yx(std_plane, (int)box_y, (int)box_x, "┌");
    for (unsigned int c = box_x + 1; c < box_x + box_w - 1; c++)
        ncplane_putstr_yx(std_plane, (int)box_y, (int)c, "─");
    ncplane_putstr_yx(std_plane, (int)box_y, (int)(box_x + box_w - 1), "┐");
    for (unsigned int r = box_y + 1; r < box_y + box_h - 1; r++) {
        ncplane_putstr_yx(std_plane, (int)r, (int)box_x, "│");
        ncplane_putstr_yx(std_plane, (int)r, (int)(box_x + box_w - 1), "│");
    }
    ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)box_x, "└");
    for (unsigned int c = box_x + 1; c < box_x + box_w - 1; c++)
        ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)c, "─");
    ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)(box_x + box_w - 1), "┘");

    ncplane_set_fg_rgb(std_plane, COL_TEXT);
    ncplane_on_styles(std_plane, NCSTYLE_BOLD);
    ncplane_printf_yx(std_plane, (int)box_y, (int)box_x + 3, " %s ",
                      is_task ? "EDIT TASK" : "EDIT PROJECT");
    ncplane_off_styles(std_plane, NCSTYLE_BOLD);

    int col_w = ((int)box_w - 8) / 2;
    if (col_w < 20) col_w = (int)box_w - 8;
    int col_field_w = col_w - 6;

    if (is_task) {
        int lx = (int)box_x + 2;
        int rx = (int)box_x + 4 + col_w;
        int y  = (int)box_y + 3;

        draw_input_field(std_plane, "Task Name", edit->name_buf, col_field_w,
                         edit->active_field == FORM_FIELD_NAME, lx, y, false);
        draw_input_field(std_plane, "Remote Host", edit->remote_host_buf, col_field_w,
                         edit->active_field == FORM_FIELD_REMOTE_HOST, rx, y, false);
        y += 3;
        draw_input_field(std_plane, "Local Path", edit->path_buf, col_field_w,
                         edit->active_field == FORM_FIELD_PATH, lx, y, false);
        draw_input_field(std_plane, "Remote User", edit->remote_user_buf, col_field_w,
                         edit->active_field == FORM_FIELD_REMOTE_USER, rx, y, false);
        y += 3;
        draw_input_field(std_plane, "Obsidian Doc (.md)", edit->obsidian_buf, col_field_w,
                         edit->active_field == FORM_FIELD_OBSIDIAN, lx, y, false);
        draw_input_field(std_plane, "Remote Password", edit->remote_pass_buf, col_field_w,
                         edit->active_field == FORM_FIELD_REMOTE_PASS, rx, y, true);
        y += 3;
        draw_input_field(std_plane, "Redmine Link", edit->redmine_buf, col_field_w,
                         edit->active_field == FORM_FIELD_REDMINE, lx, y, false);
        draw_input_field(std_plane, "Gerrit Commit", edit->gerrit_commit_buf, col_field_w,
                         edit->active_field == FORM_FIELD_GERRIT_COMMIT, rx, y, false);
        y += 3;
        draw_input_field(std_plane, "Notes", edit->notes_buf, col_field_w,
                         edit->active_field == FORM_FIELD_NOTES, lx, y, false);
    } else {
        int lx = (int)box_x + 2;
        int field_w = (int)box_w - 10;
        int y = (int)box_y + 3;
        draw_input_field(std_plane, "Project Name", edit->name_buf, field_w,
                         edit->active_field == FORM_FIELD_NAME, lx, y, false); y += 3;
        draw_input_field(std_plane, "Description", edit->description_buf, field_w,
                         edit->active_field == FORM_FIELD_PROJ_DESC, lx, y, false); y += 3;
        draw_input_field(std_plane, "Local Git Path", edit->git_path_buf, field_w,
                         edit->active_field == FORM_FIELD_PROJ_GIT_PATH, lx, y, false); y += 3;
        draw_input_field(std_plane, "Gerrit URL", edit->gerrit_url_buf, field_w,
                         edit->active_field == FORM_FIELD_PROJ_GERRIT_URL, lx, y, false); y += 3;
        draw_input_field(std_plane, "Redmine URL", edit->redmine_url_buf, field_w,
                         edit->active_field == FORM_FIELD_PROJ_REDMINE_URL, lx, y, false);
    }

    int br = (int)(box_y + box_h - 4);
    ncplane_set_fg_rgb(std_plane, COL_LINK);
    ncplane_putstr_yx(std_plane, br,     (int)box_x + 3, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, (int)box_x + 3, "│  [SAVE]  │");
    ncplane_putstr_yx(std_plane, br + 2, (int)box_x + 3, "└──────────┘");
    int cancel_x = (int)box_x + (int)box_w - 15;
    ncplane_set_fg_rgb(std_plane, COL_BORDER);
    ncplane_putstr_yx(std_plane, br,     cancel_x, "┌──────────┐");
    ncplane_putstr_yx(std_plane, br + 1, cancel_x, "│ [CANCEL] │");
    ncplane_putstr_yx(std_plane, br + 2, cancel_x, "└──────────┘");

    if (edit->error_msg[0] != '\0') {
        ncplane_set_fg_rgb(std_plane, COL_DANGER);
        ncplane_on_styles(std_plane, NCSTYLE_BOLD);
        ncplane_printf_yx(std_plane, br - 1, (int)box_x + 3, "! %s", edit->error_msg);
        ncplane_off_styles(std_plane, NCSTYLE_BOLD);
    }

    draw_form_footer(std_plane, " [Tab/↑↓] Field   [Enter] Save   [Esc] Cancel");
}

// ─────────────────────────────────────────────────────────────────────────────
// Help / Manual overlay
// ─────────────────────────────────────────────────────────────────────────────
static const char* HELP_TEXT[] = {
    "SUBASH — Project & Task Organizer",
    "",
    "Subash is a TUI that lets you organize projects and their tasks,",
    "jump into their working directories, open Obsidian docs, follow",
    "Redmine / Gerrit links, and see a compact git log for each project.",
    "",
    "GETTING STARTED",
    "  A Project is a workspace (usually a git repo). It has details:",
    "  description, local git path, Gerrit URL, Redmine URL.",
    "",
    "  A Task lives under a project. It has: name, local path,",
    "  Obsidian doc, Redmine link, Gerrit commit, remote host/user/pass,",
    "  and notes.",
    "",
    "NAVIGATION",
    "  ←/→ or h/l         Switch pane focus (projects ↔ tasks)",
    "  ↑/↓ or j/k         Move within the focused pane",
    "  Enter              cd into selected task's local_path",
    "",
    "MANAGE",
    "  a                  Add task (full-screen form)",
    "  n                  New project",
    "  e                  Edit selected task",
    "  P                  Edit selected project's details",
    "  d                  Delete selected task",
    "  D                  Delete selected project (cascades)",
    "",
    "OPEN LINKS",
    "  o                  Open task's Obsidian doc in xdg-open",
    "  g                  Open task's Gerrit commit URL",
    "  r                  Open task or project Redmine URL",
    "  c                  Copy task's local_path to clipboard",
    "  (mouse click)      Click the underlined .md path in the table",
    "",
    "SEARCH & HELP",
    "  /                  Fuzzy filter tasks in current project",
    "  ?  or  F1          Toggle this help",
    "  q / Esc            Quit",
    "",
    "CLI MODE",
    "  subash <name>      Headless: jump to the project or task",
    "                     whose name matches (bash 'cd' via IPC).",
    "",
    "CONFIG FILE",
    "  ~/.subash.conf     Plain-text, edit at will. Reloaded on launch.",
    "  Schema is line-oriented:",
    "     project = MyProject",
    "       description = short blurb",
    "       git_path    = ~/code/myproject",
    "       gerrit_url  = https://gerrit.example.com/#/q/project:MyProject",
    "       redmine_url = https://redmine.example.com/projects/myproject",
    "       product = task-A                # 'product' = task, legacy key",
    "         path         = ~/code/myproject/taskA",
    "         obsidian     = ~/Obsidian/Notes/taskA.md",
    "         remote_host  = user@10.0.0.5:/opt/app",
    "         remote_user  = user",
    "         remote_pass  = secret",
    "         redmine      = https://redmine.example.com/issues/1234",
    "         gerrit_commit= abc123def",
    "         notes        = free-form",
    "",
    "PATH EXPANSION",
    "  Any path starting with ~/ or $HOME/ is expanded when used.",
    "  Stored values keep the ~ prefix so the file stays portable.",
    "",
    "STATUS DOTS in the task table",
    "  ● green    local_path exists",
    "  ○ yellow   remote-only (no local_path, remote_host set)",
    "  ○ grey     path missing",
    "",
    "GIT LOG PANE",
    "  Shows `git -C <git_path> log --graph --oneline -n 40` for the",
    "  selected project. Set the project's Local Git Path via [P].",
    NULL,
};

void draw_help(struct notcurses* nc, struct ncplane* std_plane,
               const HelpState* help) {
    (void)nc;
    fill_form_bg(std_plane);

    unsigned int rows, cols;
    ncplane_dim_yx(std_plane, &rows, &cols);

    unsigned int box_y = 3, box_x = 4;
    unsigned int box_w = (cols > 8) ? cols - 8 : cols;
    unsigned int box_h = (rows > 6) ? rows - 6 : rows;

    ncplane_set_bg_rgb(std_plane, COL_BG_HIGH);
    for (unsigned int r = box_y; r < box_y + box_h; r++)
        for (unsigned int c = box_x; c < box_x + box_w; c++)
            ncplane_putchar_yx(std_plane, (int)r, (int)c, ' ');

    ncplane_set_fg_rgb(std_plane, COL_ACCENT);
    ncplane_putstr_yx(std_plane, (int)box_y, (int)box_x, "┌");
    for (unsigned int c = box_x + 1; c < box_x + box_w - 1; c++)
        ncplane_putstr_yx(std_plane, (int)box_y, (int)c, "─");
    ncplane_putstr_yx(std_plane, (int)box_y, (int)(box_x + box_w - 1), "┐");
    for (unsigned int r = box_y + 1; r < box_y + box_h - 1; r++) {
        ncplane_putstr_yx(std_plane, (int)r, (int)box_x, "│");
        ncplane_putstr_yx(std_plane, (int)r, (int)(box_x + box_w - 1), "│");
    }
    ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)box_x, "└");
    for (unsigned int c = box_x + 1; c < box_x + box_w - 1; c++)
        ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)c, "─");
    ncplane_putstr_yx(std_plane, (int)(box_y + box_h - 1), (int)(box_x + box_w - 1), "┘");

    ncplane_set_fg_rgb(std_plane, COL_TEXT);
    ncplane_on_styles(std_plane, NCSTYLE_BOLD);
    ncplane_printf_yx(std_plane, (int)box_y, (int)box_x + 3, " MANUAL ");
    ncplane_off_styles(std_plane, NCSTYLE_BOLD);

    int content_max = (int)box_h - 3;
    int line_i = 0;
    int shown = 0;
    for (int i = 0; HELP_TEXT[i] != NULL && shown < content_max; i++) {
        if (line_i < help->scroll_offset) { line_i++; continue; }
        const char *line = HELP_TEXT[i];
        // Line color:
        uint32_t c;
        if (line[0] == '\0') c = COL_TEXT_DIM;
        else if (line[0] != ' ' && line[0] != '\t') c = COL_ACCENT;  // section header
        else c = COL_TEXT;
        ncplane_set_fg_rgb(std_plane, c);
        ncplane_printf_yx(std_plane, (int)box_y + 2 + shown, (int)box_x + 3, "%s", line);
        shown++;
        line_i++;
    }

    draw_form_footer(std_plane, " [j/k] Scroll   [Esc / ?] Close");
}

// ─────────────────────────────────────────────────────────────────────────────
// Filter overlay
// ─────────────────────────────────────────────────────────────────────────────
void draw_filter_overlay(struct notcurses* nc, struct ncplane* std_plane,
                         const FilterState* filter) {
    (void)nc;
    unsigned int rows, cols;
    ncplane_dim_yx(std_plane, &rows, &cols);
    int y = (int)rows - 3;
    // clear a strip
    ncplane_set_bg_rgb(std_plane, COL_BG_HIGH);
    for (unsigned int c = 0; c < cols; c++)
        ncplane_putchar_yx(std_plane, y, (int)c, ' ');
    ncplane_set_fg_rgb(std_plane, COL_WARN);
    ncplane_on_styles(std_plane, NCSTYLE_BOLD);
    ncplane_putstr_yx(std_plane, y, 2, "/");
    ncplane_off_styles(std_plane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb(std_plane, COL_TEXT);
    ncplane_printf(std_plane, "%s", filter->query);
    ncplane_set_bg_rgb(std_plane, COL_ACCENT);
    ncplane_set_fg_rgb(std_plane, COL_BG);
    ncplane_putchar(std_plane, ' ');
    ncplane_set_bg_rgb(std_plane, COL_BG);
    ncplane_set_fg_rgb(std_plane, COL_TEXT_DIM);
    ncplane_printf_yx(std_plane, y, (int)cols - 30, "  [Enter] pick  [Esc] clear");
}
