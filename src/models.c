#include "models.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void get_config_path(char *path, size_t max_len) {
    const char *home = getenv("HOME");
    if (!home) {
        home = ".";
    }
    snprintf(path, max_len, "%s/.subash.conf", home);
}

static void trim(char *str) {
    size_t len = strlen(str);
    // Trim trailing whitespace
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
    // Trim leading whitespace
    size_t start = 0;
    while (str[start] && isspace((unsigned char)str[start])) {
        start++;
    }
    if (start > 0) {
        memmove(str, str + start, len - start + 1);
    }
}

static int initialize_default_template(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        return -1;
    }
    fprintf(f, "# subash configuration file\n");
    fprintf(f, "# Organize your workspace projects and their products here.\n\n");
    fprintf(f, "selected_index = 0\n\n");
    fprintf(f, "project = subash\n");
    fprintf(f, "  product = core\n");
    fprintf(f, "    path = /home/rajeevkr945/bashscripts/AI-Assistant\n");
    fprintf(f, "    ssh = git@github.com:rajeevkr945/dev-flow.git\n");
    fprintf(f, "    link = https://github.com/rajeevkr945/dev-flow\n");
    fclose(f);
    return 0;
}

int load_config(AppState *state) {
    // Reset state
    memset(state, 0, sizeof(AppState));
    state->selected_index = 0;
    state->should_quit = false;

    char path[512];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        // Initialize with default template if not present
        if (initialize_default_template(path) != 0) {
            return -1;
        }
        f = fopen(path, "r");
        if (!f) {
            return -1;
        }
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        // Remove comments
        char *comment = strchr(line, '#');
        if (comment) {
            *comment = '\0';
        }
        comment = strchr(line, ';');
        if (comment) {
            *comment = '\0';
        }

        trim(line);
        if (line[0] == '\0') {
            continue;
        }

        char *equal = strchr(line, '=');
        if (!equal) {
            continue;
        }

        *equal = '\0';
        char *key = line;
        char *val = equal + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "selected_index") == 0) {
            state->selected_index = atoi(val);
        } else if (strcmp(key, "project") == 0) {
            if (state->project_count < MAX_PROJECTS) {
                Project *proj = &state->projects[state->project_count];
                memset(proj, 0, sizeof(Project));
                strncpy(proj->name, val, MAX_NAME - 1);
                proj->name[MAX_NAME - 1] = '\0';
                state->project_count++;
            }
        } else if (strcmp(key, "product") == 0) {
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
        } else if (strcmp(key, "path") == 0) {
            if (state->project_count > 0) {
                Project *proj = &state->projects[state->project_count - 1];
                if (proj->product_count > 0) {
                    Product *prod = &proj->products[proj->product_count - 1];
                    strncpy(prod->local_path, val, MAX_PATH - 1);
                    prod->local_path[MAX_PATH - 1] = '\0';
                }
            }
        } else if (strcmp(key, "ssh") == 0) {
            if (state->project_count > 0) {
                Project *proj = &state->projects[state->project_count - 1];
                if (proj->product_count > 0) {
                    Product *prod = &proj->products[proj->product_count - 1];
                    strncpy(prod->remote_ssh, val, MAX_SSH - 1);
                    prod->remote_ssh[MAX_SSH - 1] = '\0';
                }
            }
        } else if (strcmp(key, "link") == 0) {
            if (state->project_count > 0) {
                Project *proj = &state->projects[state->project_count - 1];
                if (proj->product_count > 0) {
                    Product *prod = &proj->products[proj->product_count - 1];
                    if (prod->link_count < MAX_LINKS) {
                        strncpy(prod->links[prod->link_count], val, MAX_LINK_LEN - 1);
                        prod->links[prod->link_count][MAX_LINK_LEN - 1] = '\0';
                        prod->link_count++;
                    }
                }
            }
        }
    }

    fclose(f);
    return 0;
}

int save_config(const AppState *state) {
    char path[512];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) {
        return -1;
    }

    fprintf(f, "# subash configuration file\n");
    fprintf(f, "# Generated automatically. Feel free to edit.\n\n");
    fprintf(f, "selected_index = %d\n\n", state->selected_index);

    for (int i = 0; i < state->project_count; i++) {
        const Project *proj = &state->projects[i];
        fprintf(f, "project = %s\n", proj->name);
        for (int j = 0; j < proj->product_count; j++) {
            const Product *prod = &proj->products[j];
            fprintf(f, "  product = %s\n", prod->name);
            if (prod->local_path[0] != '\0') {
                fprintf(f, "    path = %s\n", prod->local_path);
            }
            if (prod->remote_ssh[0] != '\0') {
                fprintf(f, "    ssh = %s\n", prod->remote_ssh);
            }
            for (int k = 0; k < prod->link_count; k++) {
                fprintf(f, "    link = %s\n", prod->links[k]);
            }
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
                if (out_project) {
                    *out_project = proj;
                }
                return (Product *)&proj->products[j];
            }
            current++;
        }
    }
    return NULL;
}

int get_total_product_count(const AppState *state) {
    int total = 0;
    for (int i = 0; i < state->project_count; i++) {
        total += state->projects[i].product_count;
    }
    return total;
}
