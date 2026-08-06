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
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
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
    fprintf(f, "selected_project = 0\n");
    fprintf(f, "selected_product = 0\n\n");
    fprintf(f, "project = subash\n");
    fprintf(f, "  product = core\n");
    fprintf(f, "    path = /home/rajeev/Documents/Aurora\n");
    fprintf(f, "    ssh = git@github.com:rajeevkr945/dev-flow.git\n");
    fprintf(f, "    link = https://github.com/rajeevkr945/AI-Assistant/tree/TUI_port_to_C\n");
    fprintf(f, "    tasks =\n");
    fprintf(f, "    notes =\n");
    fclose(f);
    return 0;
}

int load_config(AppState *state) {
    memset(state, 0, sizeof(AppState));
    state->selected_project = 0;
    state->selected_product = 0;
    state->nav_focus = NAV_FOCUS_PRODUCT;
    state->should_quit = false;

    char path[512];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        if (initialize_default_template(path) != 0) {
            return -1;
        }
        f = fopen(path, "r");
        if (!f) {
            return -1;
        }
    }

    // Legacy flat selected_index → applied after load if present
    int legacy_flat = -1;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        size_t llen = strlen(line);
        if (llen > 0 && line[llen - 1] == '\n') line[--llen] = '\0';
        if (llen > 0 && line[llen - 1] == '\r') line[--llen] = '\0';

        char *equal = strchr(line, '=');
        if (!equal) {
            char *comment = strchr(line, '#');
            if (comment) *comment = '\0';
            comment = strchr(line, ';');
            if (comment) *comment = '\0';
        } else {
            char save = *equal;
            *equal = '\0';
            char *comment = strchr(line, '#');
            if (comment) *comment = '\0';
            comment = strchr(line, ';');
            if (comment) *comment = '\0';
            *equal = save;
        }

        trim(line);
        if (line[0] == '\0') {
            continue;
        }

        equal = strchr(line, '=');
        if (!equal) {
            continue;
        }

        *equal = '\0';
        char *key = line;
        char *val = equal + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "selected_index") == 0) {
            legacy_flat = atoi(val);
        } else if (strcmp(key, "selected_project") == 0) {
            state->selected_project = atoi(val);
        } else if (strcmp(key, "selected_product") == 0) {
            state->selected_product = atoi(val);
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
        } else if (strcmp(key, "tasks") == 0) {
            if (state->project_count > 0) {
                Project *proj = &state->projects[state->project_count - 1];
                if (proj->product_count > 0) {
                    Product *prod = &proj->products[proj->product_count - 1];
                    strncpy(prod->tasks, val, MAX_TASKS - 1);
                    prod->tasks[MAX_TASKS - 1] = '\0';
                }
            }
        } else if (strcmp(key, "notes") == 0) {
            if (state->project_count > 0) {
                Project *proj = &state->projects[state->project_count - 1];
                if (proj->product_count > 0) {
                    Product *prod = &proj->products[proj->product_count - 1];
                    strncpy(prod->notes, val, MAX_NOTES - 1);
                    prod->notes[MAX_NOTES - 1] = '\0';
                }
            }
        }
    }

    fclose(f);

    // Migrate legacy flat selected_index if present and new keys were default
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
    if (!f) {
        return -1;
    }

    fprintf(f, "# subash configuration file\n");
    fprintf(f, "# Generated automatically. Feel free to edit.\n\n");
    fprintf(f, "selected_project = %d\n", state->selected_project);
    fprintf(f, "selected_product = %d\n\n", state->selected_product);

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
            if (prod->tasks[0] != '\0') {
                fprintf(f, "    tasks = %s\n", prod->tasks);
            }
            if (prod->notes[0] != '\0') {
                fprintf(f, "    notes = %s\n", prod->notes);
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

Product *get_selected_product(const AppState *state, const Project **out_project) {
    if (state->selected_project < 0 || state->selected_project >= state->project_count) {
        if (out_project) *out_project = NULL;
        return NULL;
    }
    const Project *proj = &state->projects[state->selected_project];
    if (out_project) *out_project = proj;
    if (state->selected_product < 0 || state->selected_product >= proj->product_count) {
        return NULL;
    }
    return (Product *)&proj->products[state->selected_product];
}

int get_selected_flat_index(const AppState *state) {
    if (state->selected_project < 0 || state->selected_project >= state->project_count)
        return -1;
    int flat = 0;
    for (int i = 0; i < state->selected_project; i++)
        flat += state->projects[i].product_count;
    const Project *proj = &state->projects[state->selected_project];
    if (state->selected_product < 0 || state->selected_product >= proj->product_count)
        return -1;
    return flat + state->selected_product;
}

void normalize_selection(AppState *state) {
    if (state->project_count <= 0) {
        state->selected_project = 0;
        state->selected_product = 0;
        return;
    }
    if (state->selected_project < 0)
        state->selected_project = 0;
    if (state->selected_project >= state->project_count)
        state->selected_project = state->project_count - 1;

    Project *proj = &state->projects[state->selected_project];
    if (proj->product_count <= 0) {
        state->selected_product = 0;
        return;
    }
    if (state->selected_product < 0)
        state->selected_product = 0;
    if (state->selected_product >= proj->product_count)
        state->selected_product = proj->product_count - 1;
}

int find_project_by_name(const AppState *state, const char *name) {
    if (!name || name[0] == '\0') return -1;
    for (int i = 0; i < state->project_count; i++) {
        if (strcmp(state->projects[i].name, name) == 0)
            return i;
    }
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
                         const char *tasks, const char *link, const char *notes) {
    if (project_index < 0 || project_index >= state->project_count) {
        return -1;
    }

    Project *proj = &state->projects[project_index];
    if (proj->product_count >= MAX_PRODUCTS) {
        return -1;
    }

    Product *prod = &proj->products[proj->product_count];
    memset(prod, 0, sizeof(Product));
    strncpy(prod->name, name, MAX_NAME - 1);
    prod->name[MAX_NAME - 1] = '\0';
    if (local_path) {
        strncpy(prod->local_path, local_path, MAX_PATH - 1);
        prod->local_path[MAX_PATH - 1] = '\0';
    }
    if (tasks) {
        strncpy(prod->tasks, tasks, MAX_TASKS - 1);
        prod->tasks[MAX_TASKS - 1] = '\0';
    }
    if (notes) {
        strncpy(prod->notes, notes, MAX_NOTES - 1);
        prod->notes[MAX_NOTES - 1] = '\0';
    }
    if (link && link[0] != '\0') {
        strncpy(prod->links[0], link, MAX_LINK_LEN - 1);
        prod->links[0][MAX_LINK_LEN - 1] = '\0';
        prod->link_count = 1;
    }
    proj->product_count++;

    return 0;
}
