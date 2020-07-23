#include "connection.h"

ConnectionManager *init_connection_manager() {
    ConnectionManager *cm = safe_malloc(sizeof(ConnectionManager));
    cm->occupied_connections = NULL;
    cm->unused_connections = NULL;
    pthread_mutex_init(&cm->lock, NULL);
    return cm;
}

ActiveConnection *new_active_connection(ConnectionManager *cm, int fd,
        enum ConnectionStatus status) {
    if (!cm) {
        return NULL;
    }

    pthread_mutex_lock(&cm->lock);

    // new instance
    ActiveConnection *new_ac = NULL;
    if (!cm->unused_connections) {
        new_ac = safe_malloc(sizeof(ActiveConnection));
    } else {
        new_ac = cm->unused_connections;
        cm->unused_connections = new_ac->next;
        if (new_ac->next) {
            cm->unused_connections->prev = NULL;
        }
    }

    // unlink
    new_ac->next = NULL;
    new_ac->prev = NULL;

    // insert into occupied list
    if (cm->occupied_connections) {
        new_ac->next = cm->occupied_connections;
        cm->occupied_connections->prev = new_ac;
    }
    cm->occupied_connections = new_ac;

    // init
    new_ac->stat = status;
    new_ac->fd = fd;
    new_ac->data = init_request_data();

    pthread_mutex_unlock(&cm->lock);
    return new_ac;
}

void destroy_active_connection(ConnectionManager *cm, ActiveConnection *ac) {
    if (!ac || !cm) {
        return;
    }

    pthread_mutex_lock(&cm->lock);

    // remove from linked list
    if (ac->prev) {
        ac->prev->next = ac->next;
    } else {
        cm->occupied_connections = ac->next;
    }
    if (ac->next) {
        ac->next->prev = ac->prev;
    }

    // unlink
    ac->next = NULL;
    ac->prev = NULL;

    // release unused memory
    close(ac->fd);
    if (ac->stat == Request) {
        destroy_reading_data(ac->data);
    } else {
        destroy_responce_data(ac->data);
    }

    // add to unused
    ac->next = cm->unused_connections;
    if (cm->unused_connections) {
        ac->next->prev = ac;
    }
    cm->unused_connections = ac;

    pthread_mutex_unlock(&cm->lock);
    return;

}

void destroy_connection_manager(ConnectionManager *cm) {
    if (!cm) {
        return;
    }

    pthread_mutex_lock(&cm->lock);

    // release all occupied
    ActiveConnection *ac = cm->occupied_connections;
    ActiveConnection *prev = NULL;
    while (ac) {
        close(ac->fd);
        if (ac->stat == Request) {
            destroy_reading_data(ac->data);
        } else {
            destroy_responce_data(ac->data);
        }
        prev = ac;
        ac = ac->next;
        free(prev);
    }

    // release all unused
    ac = cm->unused_connections;
    prev = NULL;
    while (ac) {
        prev = ac;
        ac = ac->next;
        free(prev);
    }

    pthread_mutex_unlock(&cm->lock);
    free(cm);
    return;
}