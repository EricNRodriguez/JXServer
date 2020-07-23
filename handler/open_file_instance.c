#include "open_file_instance.h"

int init_open_file_instances(OpenFileInstances **ofis) {
    if (!ofis) {
        return -1;
    }

    (*ofis)->open_file_instances = safe_malloc(sizeof(OpenFileInstance *) *
            OPEN_FILE_INSTANCES_INIT_LEN);
    (*ofis)->instances_len = OPEN_FILE_INSTANCES_INIT_LEN;
    (*ofis)->n_instances = 0;
    pthread_mutex_init(&(*ofis)->lock, NULL);
    return 0;
}

int init_open_file_instance(OpenFileInstance **ofi, char *file_path,
        uint32_t session_id, uint64_t offset, uint64_t n_requested) {
    // error file doesnt exist
    if (!file_path || access(file_path, F_OK) || !ofi) {
        return -1;
    }

    *ofi = safe_malloc(sizeof(OpenFileInstance));
    (*ofi)->session_id = session_id;
    (*ofi)->n_requested = n_requested;
    (*ofi)->offset = offset;
    (*ofi)->n_read = 0;
    (*ofi)->reference_count = 1;

    (*ofi)->file = fopen(file_path, "rb+");
    // adv file pointer by the offset
    fseek((*ofi)->file, offset, SEEK_SET);

    // copy file path
    (*ofi)->file_path = safe_malloc(strlen(file_path) + 1);
    memcpy((*ofi)->file_path, file_path, strlen(file_path) + 1);

    pthread_mutex_init(&(*ofi)->lock, NULL);
    return 0;
}

int open_file(OpenFileInstance **ofi, OpenFileInstances *ofis, char *file_path,
        uint32_t session_id, uint64_t offset, uint64_t n_requested) {
    if (!ofis || !file_path) {
        return -1;
    }

    if (access(file_path, F_OK)) {
        return -1;
    }

    ssize_t unused_index = -1;
    for (size_t i = 0; i < ofis->n_instances; ++i) {
        if (unused_index == -1 && !ofis->open_file_instances[i]->reference_count) {
            unused_index = i;
        }
        if (ofis->open_file_instances[i]->reference_count &&
            ofis->open_file_instances[i]->session_id == session_id) {
            if (strcmp(ofis->open_file_instances[i]->file_path, file_path)) {
                // same session id, different file paths
                return -1;
            } else if (ofis->open_file_instances[i]->offset != offset ||
                       ofis->open_file_instances[i]->n_requested != n_requested) {
                // same session id and file path, different byte ranges
                return -1;
            } else {
                // multiplex request!
                *ofi = ofis->open_file_instances[i];
                ofis->open_file_instances[i]->reference_count++;
                return 0;
            }
        }
    }

    if (unused_index == -1) {
        if (ofis->n_instances >= ofis->instances_len) {
            ofis->open_file_instances = safe_realloc(ofis->open_file_instances,
                    ARRAY_GROWTH_RATE * ofis->instances_len);
            ofis->instances_len *= ARRAY_GROWTH_RATE;
        }
        unused_index = ofis->n_instances;
        ofis->n_instances++;
    } else {
        // destroy old ofi
        destroy_open_file_instance(ofis->open_file_instances[unused_index]);
    }

    // new open file instance
    init_open_file_instance(&ofis->open_file_instances[unused_index], file_path,
            session_id, offset, n_requested);
    // initialise passed ofi addr
    *ofi = ofis->open_file_instances[unused_index];

    return 0;
}

void destroy_open_file_instance(OpenFileInstance *ofi) {
    if (!ofi) {
        return;
    }

    free(ofi->file_path);
    fclose(ofi->file);
    pthread_mutex_destroy(&ofi->lock);
    free(ofi);
    return;
}

void destroy_open_file_instances(OpenFileInstances *ofis) {
    if (!ofis) {
        return;
    }

    for (size_t i = 0; i < ofis->n_instances; ++i) {
        destroy_open_file_instance(ofis->open_file_instances[i]);
    }
    free(ofis->open_file_instances);

    free(ofis);
    return;
}