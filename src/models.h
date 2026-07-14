#ifndef MODELS_H
#define MODELS_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_NAME 64
#define MAX_PATH 512
#define MAX_SSH 128
#define MAX_LINKS 8
#define MAX_LINK_LEN 128
#define MAX_PRODUCTS 16
#define MAX_PROJECTS 32

typedef struct {
    char name[MAX_NAME];
    char local_path[MAX_PATH];
    char remote_ssh[MAX_SSH];
    char redmine[MAX_LINK_LEN];   // Redmine project URL / reference
    char links[MAX_LINKS][MAX_LINK_LEN];
    int link_count;
} Product;

typedef struct {
    char name[MAX_NAME];
    Product products[MAX_PRODUCTS];
    int product_count;
} Project;

typedef struct {
    Project projects[MAX_PROJECTS];
    int project_count;
    int selected_index;
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

// Add a new product to the project at the given index in AppState.
// Returns 0 on success, -1 if project_index is out of range or the project is full.
int add_product_to_state(AppState *state, int project_index, const char *name, const char *local_path);

#endif // MODELS_H
