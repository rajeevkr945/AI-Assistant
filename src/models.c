#include "models.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void get_config_path(char *path, size_t max_len) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, max_len, "%s/.subash.conf", home);
}

static void trim(char *str) {
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
    size_t start = 0;
    while (str[start] && isspace((unsigned char)str[start])) start++;
    if (start > 0) memmove(str, str + start, len - start + 1);
}

static int initialize_default_template(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# subash configuration file\n");
    fprintf(f, "# Organize your projects and tasks here.\n\n");
    fprintf(f, "selected_project = 0\n");
    fprintf(f, "selected_product = 0\n\n");
    fprintf(f, "project = subash\n");
    fprintf(f, "  description = terminal task organizer\n");
    fprintf(f, "  git_path = %s\n", getenv("PWD") ? getenv("PWD") : ".");
    fprintf(f, "  product = core\n");
    fprintf(f, "    path = %s\n", getenv("PWD") ? getenv("PWD") : ".");
    fprintf(f, "    notes =\n");
    fclose(f);
    return 0;
}

// Assign a value to the "current" (last-declared) project/product context.
static void assign_project_field(Project *proj, const char *key, const char *val) {
    if      (strcmp(key, "description")    == 0) { strncpy(proj->description,    val, MAX_DESC-1);     proj->description[MAX_DESC-1] = '\0'; }
    else if (strcmp(key, "gerrit_url")     == 0) { strncpy(proj->gerrit_url,     val, MAX_LINK_LEN-1); proj->gerrit_url[MAX_LINK_LEN-1] = '\0'; }
    else if (strcmp(key, "git_path")       == 0) { strncpy(proj->local_git_path, val, MAX_PATH-1);     proj->local_git_path[MAX_PATH-1] = '\0'; }
    else if (strcmp(key, "redmine_url")    == 0) { strncpy(proj->redmine_url,    val, MAX_LINK_LEN-1); proj->redmine_url[MAX_LINK_LEN-1] = '\0'; }
    else if (strcmp(key, "obsidian_sync")  == 0) { strncpy(proj->obsidian_sync,  val, MAX_PATH-1);     proj->obsidian_sync[MAX_PATH-1] = '\0'; }
}

static void assign_product_field(Product *prod, const char *key, const char *val) {
    if      (strcmp(key, "path")          == 0) { strncpy(prod->local_path,    val, MAX_PATH-1);     prod->local_path[MAX_PATH-1] = '\0'; }
    else if (strcmp(key, "obsidian")      == 0) { strncpy(prod->obsidian_path, val, MAX_PATH-1);     prod->obsidian_path[MAX_PATH-1] = '\0'; }
    else if (strcmp(key, "remote_host")   == 0) { strncpy(prod->remote_host,   val, MAX_PATH-1);     prod->remote_host[MAX_PATH-1] = '\0'; }
    else if (strcmp(key, "remote_user")   == 0) { strncpy(prod->remote_user,   val, MAX_NAME-1);     prod->remote_user[MAX_NAME-1] = '\0'; }
    else if (strcmp(key, "remote_pass")   == 0) { strncpy(prod->remote_pass,   val, MAX_NAME-1);     prod->remote_pass[MAX_NAME-1] = '\0'; }
    else if (strcmp(key, "ssh")           == 0) { /* legacy — kept as-is */ }
    else if (strcmp(key, "redmine")       == 0) { strncpy(prod->redmine,       val, MAX_LINK_LEN-1); prod->redmine[MAX_LINK_LEN-1] = '\0'; }
    else if (strcmp(key, "gerrit_commit") == 0) { strncpy(prod->gerrit_commit, val, MAX_LINK_LEN-1); prod->gerrit_commit[MAX_LINK_LEN-1] = '\0'; }
    else if (strcmp(key, "link")          == 0) {
        if (prod->link_count < MAX_LINKS) {
            strncpy(prod->links[prod->link_count], val, MAX_LINK_LEN-1);
            prod->links[prod->link_count][MAX_LINK_LEN-1] = '\0';
            prod->link_count++;
        }
    }
    else if (strcmp(key, "tasks") == 0) { strncpy(prod->tasks, val, MAX_TASKS-1); prod->tasks[MAX_TASKS-1] = '\0'; }
    else if (strcmp(key, "notes") == 0) { strncpy(prod->notes, val, MAX_NOTES-1); prod->notes[MAX_NOTES-1] = '\0'; }
}

int load_config(AppState *state) {
    memset(state, 0, sizeof(AppState));
    state->selected_project = 0;
    state->selected_product = 0;
    state->nav_focus = NAV_FOCUS_PRODUCT;
    state->should_quit = false;
    state->git_graph_project_idx = -1;

    char path[512];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        if (initialize_default_template(path) != 0) return -1;
        f = fopen(path, "r");
        if (!f) return -1;
    }

    int legacy_flat = -1;
    char line[2048];

    while (fgets(line, sizeof(line), f)) {
        size_t llen = strlen(line);
        if (llen > 0 && line[llen - 1] == '\n') line[--llen] = '\0';
        if (llen > 0 && line[llen - 1] == '\r') line[--llen] = '\0';

        // strip comments (# / ;) outside of value
        char *equal = strchr(line, '=');
        if (!equal) {
            char *c = strchr(line, '#'); if (c) *c = '\0';
            c = strchr(line, ';');       if (c) *c = '\0';
        } else {
            char save = *equal; *equal = '\0';
            char *c = strchr(line, '#'); if (c) *c = '\0';
            c = strchr(line, ';');       if (c) *c = '\0';
            *equal = save;
        }
        trim(line);
        if (line[0] == '\0') continue;

        equal = strchr(line, '=');
        if (!equal) continue;
        *equal = '\0';
        char *key = line;
        char *val = equal + 1;
        trim(key); trim(val);

        if (strcmp(key, "selected_index") == 0)   { legacy_flat = atoi(val); continue; }
        if (strcmp(key, "selected_project") == 0) { state->selected_project = atoi(val); continue; }
        if (strcmp(key, "selected_product") == 0) { state->selected_product = atoi(val); continue; }

        if (strcmp(key, "project") == 0) {
            if (state->project_count < MAX_PROJECTS) {
                Project *proj = &state->projects[state->project_count];
                memset(proj, 0, sizeof(Project));
                strncpy(proj->name, val, MAX_NAME - 1);
                proj->name[MAX_NAME - 1] = '\0';
                state->project_count++;
            }
            continue;
        }
        if (strcmp(key, "product") == 0 || strcmp(key, "task") == 0) {
            if (state->project_count > 0) {
                Project *proj = &state->projects[state->project_count - 1];
                if (proj->product_count < MAX_PRODUCTS) {
                    Product *prod = &proj->products[proj->product_count];
                    memset(prod, 0, sizeof(Product));
                    strncpy(prod->name, val, MAX_NAME - 1);
                    prod->name[MAX_NAME - 1] = '\0';
                    proj->product_count++;
                }
            }
            continue;
        }

        // Everything else is a field on the current context (product if any exists,
        // else project). Product-level keys take precedence when a product is open.
        if (state->project_count == 0) continue;

        Project *proj = &state->projects[state->project_count - 1];
        if (proj->product_count > 0) {
            Product *prod = &proj->products[proj->product_count - 1];
            // If the key is a known project-level key AND no product-specific handler
            // owns it, apply to the project. Otherwise apply to the product.
            if (strcmp(key, "description") == 0 ||
                strcmp(key, "gerrit_url")  == 0 ||
                strcmp(key, "git_path")    == 0 ||
                strcmp(key, "redmine_url") == 0) {
                assign_project_field(proj, key, val);
            } else {
                assign_product_field(prod, key, val);
            }
        } else {
            // No product yet — apply project-level keys.
            assign_project_field(proj, key, val);
        }
    }

    fclose(f);

    if (legacy_flat >= 0) {
        int current = 0;
        for (int i = 0; i < state->project_count; i++) {
            for (int j = 0; j < state->projects[i].product_count; j++) {
                if (current == legacy_flat) {
                    state->selected_project = i;
                    state->selected_product = j;
                    goto done_legacy;
                }
                current++;
            }
        }
        done_legacy:;
    }

    normalize_selection(state);
    return 0;
}

int save_config(const AppState *state) {
    char path[512];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "# subash configuration file\n");
    fprintf(f, "# Generated automatically. Feel free to edit.\n\n");
    fprintf(f, "selected_project = %d\n", state->selected_project);
    fprintf(f, "selected_product = %d\n\n", state->selected_product);

    for (int i = 0; i < state->project_count; i++) {
        const Project *proj = &state->projects[i];
        fprintf(f, "project = %s\n", proj->name);
        if (proj->description[0])    fprintf(f, "  description = %s\n", proj->description);
        if (proj->gerrit_url[0])     fprintf(f, "  gerrit_url = %s\n",  proj->gerrit_url);
        if (proj->local_git_path[0]) fprintf(f, "  git_path = %s\n",    proj->local_git_path);
        if (proj->redmine_url[0])    fprintf(f, "  redmine_url = %s\n", proj->redmine_url);
        if (proj->obsidian_sync[0])  fprintf(f, "  obsidian_sync = %s\n", proj->obsidian_sync);

        for (int j = 0; j < proj->product_count; j++) {
            const Product *prod = &proj->products[j];
            fprintf(f, "  product = %s\n", prod->name);
            if (prod->local_path[0])    fprintf(f, "    path = %s\n",          prod->local_path);
            if (prod->obsidian_path[0]) fprintf(f, "    obsidian = %s\n",      prod->obsidian_path);
            if (prod->remote_host[0])   fprintf(f, "    remote_host = %s\n",   prod->remote_host);
            if (prod->remote_user[0])   fprintf(f, "    remote_user = %s\n",   prod->remote_user);
            if (prod->remote_pass[0])   fprintf(f, "    remote_pass = %s\n",   prod->remote_pass);
            if (prod->redmine[0])       fprintf(f, "    redmine = %s\n",       prod->redmine);
            if (prod->gerrit_commit[0]) fprintf(f, "    gerrit_commit = %s\n", prod->gerrit_commit);
            for (int k = 0; k < prod->link_count; k++)
                fprintf(f, "    link = %s\n", prod->links[k]);
            if (prod->tasks[0])         fprintf(f, "    tasks = %s\n", prod->tasks);
            if (prod->notes[0])         fprintf(f, "    notes = %s\n", prod->notes);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    return 0;
}

Product *get_product_at(const AppState *state, int flat_index, const Project **out_project) {
    int current = 0;
    for (int i = 0; i < state->project_count; i++) {
        const Project *proj = &state->projects[i];
        for (int j = 0; j < proj->product_count; j++) {
            if (current == flat_index) {
                if (out_project) *out_project = proj;
                return (Product *)&proj->products[j];
            }
            current++;
        }
    }
    return NULL;
}

int get_total_product_count(const AppState *state) {
    int total = 0;
    for (int i = 0; i < state->project_count; i++)
        total += state->projects[i].product_count;
    return total;
}

Product *get_selected_product(const AppState *state, const Project **out_project) {
    if (state->selected_project < 0 || state->selected_project >= state->project_count) {
        if (out_project) *out_project = NULL;
        return NULL;
    }
    const Project *proj = &state->projects[state->selected_project];
    if (out_project) *out_project = proj;
    if (state->selected_product < 0 || state->selected_product >= proj->product_count) return NULL;
    return (Product *)&proj->products[state->selected_product];
}

int get_selected_flat_index(const AppState *state) {
    if (state->selected_project < 0 || state->selected_project >= state->project_count) return -1;
    int flat = 0;
    for (int i = 0; i < state->selected_project; i++) flat += state->projects[i].product_count;
    const Project *proj = &state->projects[state->selected_project];
    if (state->selected_product < 0 || state->selected_product >= proj->product_count) return -1;
    return flat + state->selected_product;
}

void normalize_selection(AppState *state) {
    if (state->project_count <= 0) {
        state->selected_project = 0;
        state->selected_product = 0;
        return;
    }
    if (state->selected_project < 0) state->selected_project = 0;
    if (state->selected_project >= state->project_count)
        state->selected_project = state->project_count - 1;

    Project *proj = &state->projects[state->selected_project];
    if (proj->product_count <= 0) { state->selected_product = 0; return; }
    if (state->selected_product < 0) state->selected_product = 0;
    if (state->selected_product >= proj->product_count)
        state->selected_product = proj->product_count - 1;
}

int find_project_by_name(const AppState *state, const char *name) {
    if (!name || name[0] == '\0') return -1;
    for (int i = 0; i < state->project_count; i++)
        if (strcmp(state->projects[i].name, name) == 0) return i;
    return -1;
}

int ensure_project(AppState *state, const char *name) {
    int idx = find_project_by_name(state, name);
    if (idx >= 0) return idx;
    if (state->project_count >= MAX_PROJECTS) return -1;
    Project *np = &state->projects[state->project_count];
    memset(np, 0, sizeof(Project));
    strncpy(np->name, name, MAX_NAME - 1);
    np->name[MAX_NAME - 1] = '\0';
    state->project_count++;
    return state->project_count - 1;
}

int add_product_to_state(AppState *state, int project_index,
                         const char *name, const char *local_path,
                         const char *tasks, const char *link, const char *notes,
                         const char *obsidian, const char *remote_host,
                         const char *remote_user, const char *remote_pass,
                         const char *redmine, const char *gerrit_commit) {
    if (project_index < 0 || project_index >= state->project_count) return -1;
    Project *proj = &state->projects[project_index];
    if (proj->product_count >= MAX_PRODUCTS) return -1;

    Product *prod = &proj->products[proj->product_count];
    memset(prod, 0, sizeof(Product));

#define COPY(dst, src, sz) do { if ((src) && (src)[0]) { strncpy((dst), (src), (sz)-1); (dst)[(sz)-1]='\0'; } } while(0)
    COPY(prod->name,          name,          MAX_NAME);
    COPY(prod->local_path,    local_path,    MAX_PATH);
    COPY(prod->tasks,         tasks,         MAX_TASKS);
    COPY(prod->notes,         notes,         MAX_NOTES);
    COPY(prod->obsidian_path, obsidian,      MAX_PATH);
    COPY(prod->remote_host,   remote_host,   MAX_PATH);
    COPY(prod->remote_user,   remote_user,   MAX_NAME);
    COPY(prod->remote_pass,   remote_pass,   MAX_NAME);
    COPY(prod->redmine,       redmine,       MAX_LINK_LEN);
    COPY(prod->gerrit_commit, gerrit_commit, MAX_LINK_LEN);
#undef COPY

    if (link && link[0]) {
        strncpy(prod->links[0], link, MAX_LINK_LEN - 1);
        prod->links[0][MAX_LINK_LEN - 1] = '\0';
        prod->link_count = 1;
    }
    proj->product_count++;
    return 0;
}

// ── Utility helpers ─────────────────────────────────────────────────────────
void expand_home(const char *src, char *dest, size_t dest_sz) {
    if (!src || !dest || dest_sz == 0) return;
    if (src[0] == '~' && (src[1] == '/' || src[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) home = "";
        snprintf(dest, dest_sz, "%s%s", home, src + 1);
        return;
    }
    if (strncmp(src, "$HOME", 5) == 0) {
        const char *home = getenv("HOME");
        if (!home) home = "";
        snprintf(dest, dest_sz, "%s%s", home, src + 5);
        return;
    }
    strncpy(dest, src, dest_sz - 1);
    dest[dest_sz - 1] = '\0';
}

bool fuzzy_match(const char *needle, const char *haystack) {
    if (!needle || !haystack) return false;
    if (needle[0] == '\0') return true;
    // Case-insensitive subsequence match.
    const unsigned char *n = (const unsigned char*)needle;
    const unsigned char *h = (const unsigned char*)haystack;
    while (*n && *h) {
        int nc = tolower(*n);
        int hc = tolower(*h);
        if (nc == hc) n++;
        h++;
    }
    return *n == '\0';
}

// ── Obsidian vault folder sync ───────────────────────────────────────────────
#include <dirent.h>
#include <sys/stat.h>

// Returns 1 if any existing product in the project already has obsidian_path
// equal to `obs_path` (exact match after expansion).
static int product_has_obsidian(const Project *proj, const char *obs_path) {
    for (int i = 0; i < proj->product_count; i++) {
        char expanded[MAX_PATH];
        expand_home(proj->products[i].obsidian_path, expanded, sizeof(expanded));
        if (strcmp(expanded, obs_path) == 0) return 1;
        // Also compare raw stored value
        if (strcmp(proj->products[i].obsidian_path, obs_path) == 0) return 1;
    }
    return 0;
}

int sync_obsidian_folder(AppState *state, int project_index) {
    if (project_index < 0 || project_index >= state->project_count) return 0;
    Project *proj = &state->projects[project_index];
    if (proj->obsidian_sync[0] == '\0') return 0;

    // Expand path
    char vault_path[MAX_PATH];
    expand_home(proj->obsidian_sync, vault_path, sizeof(vault_path));

    DIR *dir = opendir(vault_path);
    if (!dir) return 0;

    int added = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip hidden files/dirs and . / ..
        if (entry->d_name[0] == '.') continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", vault_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        // Only sync sub-directories — each vault folder is a task workspace.
        // .md files are not synced: they have no directory to cd into and are
        // already handled by the obsidian_path / 'o' key on the parent task.
        if (!S_ISDIR(st.st_mode)) continue;
        if (proj->product_count >= MAX_PRODUCTS) break;

        // Skip if already tracked
        if (product_has_obsidian(proj, full_path)) continue;

        // Add new product: both local_path and obsidian_path point to the
        // directory so Enter (cd) and 'o' (open in Obsidian) both work.
        Product *prod = &proj->products[proj->product_count];
        memset(prod, 0, sizeof(Product));
        strncpy(prod->name,          entry->d_name, MAX_NAME - 1);
        strncpy(prod->local_path,    full_path,     MAX_PATH - 1);
        strncpy(prod->obsidian_path, full_path,     MAX_PATH - 1);
        prod->name[MAX_NAME - 1]          = '\0';
        prod->local_path[MAX_PATH - 1]    = '\0';
        prod->obsidian_path[MAX_PATH - 1] = '\0';
        proj->product_count++;
        added++;
    }

    closedir(dir);

    if (added > 0)
        save_config(state);

    return added;
}

int sync_all_obsidian(AppState *state) {
    int total = 0;
    for (int i = 0; i < state->project_count; i++)
        total += sync_obsidian_folder(state, i);
    return total;
}
