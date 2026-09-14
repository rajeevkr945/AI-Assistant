#ifndef MODELS_H
#define MODELS_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_NAME 64
#define MAX_PATH 512
#define MAX_SSH 128
#define MAX_LINKS 8
#define MAX_LINK_LEN 256
#define MAX_TASKS 256           // free-form checklist buffer (legacy, per-task)
#define MAX_NOTES 512
#define MAX_DESC 512
#define MAX_PRODUCTS 32
#define MAX_PROJECTS 128

#define MAX_GIT_GRAPH 32768

typedef struct {
    char name[MAX_NAME];
    char local_path[MAX_PATH];
    char obsidian_path[MAX_PATH];       // path to .md doc
    char remote_host[MAX_PATH];         // e.g. user@host:/path
    char remote_user[MAX_NAME];
    char remote_pass[MAX_NAME];         // plaintext (local use only)
    char redmine[MAX_LINK_LEN];         // task-level redmine link
    char gerrit_commit[MAX_LINK_LEN];   // optional commit ref/URL
    char links[MAX_LINKS][MAX_LINK_LEN];
    int link_count;
    char tasks[MAX_TASKS];              // legacy free-form checklist
    char notes[MAX_NOTES];
} Product;

typedef struct {
    char name[MAX_NAME];
    char description[MAX_DESC];
    char gerrit_url[MAX_LINK_LEN];
    char local_git_path[MAX_PATH];
    char redmine_url[MAX_LINK_LEN];
    Product products[MAX_PRODUCTS];
    int product_count;
} Project;

typedef enum {
    NAV_FOCUS_PROJECT = 0,
    NAV_FOCUS_PRODUCT = 1,
    NAV_FOCUS_GIT_LOG = 2,
} NavFocus;

typedef struct {
    Project projects[MAX_PROJECTS];
    int project_count;
    int selected_project;
    int selected_product;
    NavFocus nav_focus;
    bool should_quit;

    // Ephemeral: cached git-graph output for the currently selected project.
    // Refreshed by main when selected_project changes.
    char git_graph_cache[MAX_GIT_GRAPH];
    int  git_graph_project_idx;   // which project the cache belongs to (-1 = none)
    int  git_log_scroll;          // scroll offset in the git log
} AppState;

int load_config(AppState *state);
int save_config(const AppState *state);

Product *get_product_at(const AppState *state, int flat_index, const Project **out_project);
int get_total_product_count(const AppState *state);
Product *get_selected_product(const AppState *state, const Project **out_project);
int get_selected_flat_index(const AppState *state);
void normalize_selection(AppState *state);

int find_project_by_name(const AppState *state, const char *name);
int ensure_project(AppState *state, const char *name);

// Add a new product with full metadata.
int add_product_to_state(AppState *state, int project_index,
                         const char *name, const char *local_path,
                         const char *tasks, const char *link, const char *notes,
                         const char *obsidian, const char *remote_host,
                         const char *remote_user, const char *remote_pass,
                         const char *redmine, const char *gerrit_commit);

// Expand ~ / $HOME prefix into an absolute path (writes into dest).
// Non-destructive: source string is not modified.
void expand_home(const char *src, char *dest, size_t dest_sz);

// Best-effort case-insensitive substring/subsequence match for the filter.
bool fuzzy_match(const char *needle, const char *haystack);

#endif // MODELS_H
