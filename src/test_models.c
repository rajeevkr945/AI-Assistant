#include "models.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
    static AppState state;
    
    printf("Loading configuration...\n");
    if (load_config(&state) != 0) {
        printf("Failed to load/initialize configuration.\n");
        return 1;
    }
    
    printf("Loaded configuration successfully. Projects found: %d\n", state.project_count);
    printf("Selected Project: %d, Product: %d\n", state.selected_project, state.selected_product);
    
    for (int i = 0; i < state.project_count; i++) {
        printf("Project %d: %s\n", i + 1, state.projects[i].name);
        for (int j = 0; j < state.projects[i].product_count; j++) {
            Product *p = &state.projects[i].products[j];
            printf("  Product %d: %s\n", j + 1, p->name);
            printf("    Path: %s\n", p->local_path);
            printf("    SSH: %s\n", p->remote_ssh);
            for (int k = 0; k < p->link_count; k++) {
                printf("    Link: %s\n", p->links[k]);
            }
        }
    }
    
    // Add a test project and save it
    printf("\nAdding a test project...\n");
    if (state.project_count < MAX_PROJECTS) {
        Project *new_proj = &state.projects[state.project_count];
        memset(new_proj, 0, sizeof(Project));
        strcpy(new_proj->name, "TestProject");
        
        Product *new_prod = &new_proj->products[0];
        strcpy(new_prod->name, "TestProduct");
        strcpy(new_prod->local_path, "/tmp/test-path");
        strcpy(new_prod->remote_ssh, "git@github.com:test/test.git");
        strcpy(new_prod->links[0], "https://github.com/test/test");
        new_prod->link_count = 1;
        new_proj->product_count = 1;
        state.project_count++;
    }
    state.selected_project = 1;
    state.selected_product = 0;
    
    printf("Saving configuration...\n");
    if (save_config(&state) != 0) {
        printf("Failed to save configuration.\n");
        return 1;
    }
    
    // Reload state and verify
    static AppState reloaded;
    printf("Reloading configuration...\n");
    if (load_config(&reloaded) != 0) {
        printf("Failed to reload configuration.\n");
        return 1;
    }
    
    printf("Reloaded selected_project: %d (expected 1)\n", reloaded.selected_project);
    assert(reloaded.selected_project == 1);
    
    // Find the TestProject
    int found = 0;
    for (int i = 0; i < reloaded.project_count; i++) {
        if (strcmp(reloaded.projects[i].name, "TestProject") == 0) {
            found = 1;
            assert(reloaded.projects[i].product_count == 1);
            assert(strcmp(reloaded.projects[i].products[0].name, "TestProduct") == 0);
            assert(strcmp(reloaded.projects[i].products[0].local_path, "/tmp/test-path") == 0);
            break;
        }
    }
    assert(found == 1);
    printf("All assertions passed! Test complete.\n");
    
    return 0;
}
