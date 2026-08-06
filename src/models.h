#ifndef MODELS_H
#define MODELS_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_NAME 64
#define MAX_PATH 512
#define MAX_SSH 128
#define MAX_LINKS 8
#define MAX_LINK_LEN 256
#define MAX_TASKS 256
#define MAX_NOTES 512
#define MAX_PRODUCTS 32
#define MAX_PROJECTS 128

typedef struct {
    char name[MAX_NAME];
    char local_path[MAX_PATH];
    char remote_ssh[MAX_SSH];
    char redmine[MAX_LINK_LEN];   // Redmine project URL / reference
    char links[MAX_LINKS][MAX_LINK_LEN];
    int link_count;
    char tasks[MAX_TASKS];        // free-form task list / checklist text
    char notes[MAX_NOTES];        // free-form notes
} Product;

typedef struct {
    char name[MAX_NAME];
    Product products[MAX_PRODUCTS];
    int product_count;
} Project;

typedef enum {
    NAV_FOCUS_PROJECT = 0,  // left/right: focus on project list
    NAV_FOCUS_PRODUCT = 1,  // left/right: focus on product list (registry)
} NavFocus;

typedef struct {
    Project projects[MAX_PROJECTS];
    int project_count;
    int selected_project;   // index into projects[]
    int selected_product;   // index into projects[selected_project].products[]
    NavFocus nav_focus;     // project list vs product list
    bool should_quit;
} AppState;

// Load configuration from ~/.subash.conf. If it does not exist, initialize a default blank template.
// Returns 0 on success, non-zero on failure.
int load_config(AppState *state);

// Save configuration to ~/.subash.conf.
// Returns 0 on success, non-zero on failure.
int save_config(const AppState *state);

// Helper to get the product at a flat index across all projects
Product *get_product_at(const AppState *state, int flat_index, const Project **out_project);

// Helper to get the total number of products across all projects
int get_total_product_count(const AppState *state);

// Currently selected product (within selected_project), or NULL.
Product *get_selected_product(const AppState *state, const Project **out_project);

// Flat index of the currently selected product, or -1 if none.
int get_selected_flat_index(const AppState *state);

// Clamp / repair selection indices after load or mutation.
void normalize_selection(AppState *state);

// Find project by name (case-sensitive). Returns index or -1.
int find_project_by_name(const AppState *state, const char *name);

// Ensure a project with the given name exists; returns its index or -1 on full.
int ensure_project(AppState *state, const char *name);

// Add a new product into project_index with full metadata.
// Returns 0 on success, -1 if project_index is out of range or the project is full.
int add_product_to_state(AppState *state, int project_index,
                         const char *name, const char *local_path,
                         const char *tasks, const char *link, const char *notes);

#endif // MODELS_H
