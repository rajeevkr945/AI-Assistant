#include "ipc.h"
#include <stdio.h>

int trigger_bash_action(const char *path) {
    FILE *f = fopen("/tmp/subash_action", "w");
    if (!f) {
        return -1;
    }
    
    // Single quotes are used to safely handle paths containing spaces or special characters
    if (fprintf(f, "cd '%s' && ls\n", path) < 0) {
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}
