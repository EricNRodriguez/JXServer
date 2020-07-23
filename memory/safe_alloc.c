#include "memory.h"

void *safe_malloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        perror("malloc failed\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *safe_calloc(size_t nmeb, size_t size) {
    void *p = calloc(nmeb, size);
    if (!p) {
        perror("calloc failed\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *safe_realloc(void *old, size_t size) {
    void *p = realloc(old, size);
    if (!p) {
        perror("realloc failed\n");
        exit(EXIT_FAILURE);
    }
    return p;
}