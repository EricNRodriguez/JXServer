#ifndef ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_CONNECTION_H
#define ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_CONNECTION_H

#include "../memory/memory.h"
#include "request.h"
#include "responce.h"

#include <pthread.h>

#define INIT_NUM_UNUSED_CONNECTIONS 10

enum ConnectionStatus {
    Request = 0,
    Responce = 1
};

typedef struct active_connection {
    enum ConnectionStatus stat;
    int fd; // client file descriptor
    void *data; // cast based on stat
    struct active_connection *next;
    struct active_connection *prev;
} ActiveConnection;

typedef struct {
    ActiveConnection *occupied_connections;
    ActiveConnection *unused_connections;
    pthread_mutex_t lock;
} ConnectionManager;

/** @brief Initialises connection manager.
 *
 *  Connection manager maintains a resource pool of ActiveConnection
 *  instances. If no unused instances are available, a new instance
 *  will be allocated.
 *
 *  @return ConnectionManager instance.
 */
ConnectionManager *init_connection_manager();

/** @brief Returns a new active connection instance.
 *
 *  Instance is retrieved from unused instance pool if possible.
 *  Otherwise, new instance is allocated. If cm is NULL, nothing is done
 *  and NULL is returned. Prev and Next fields of returned Instance are NULL.
 *
 *  @param cm : ConnectionManager instance.
 *  @param fd : client file descriptor.
 *  @param status : Connection status.
 *  @return ActiveConnection instance.
 */
ActiveConnection *new_active_connection(ConnectionManager *cm, int fd,
        enum ConnectionStatus status);

/** @brief Releases an active connection instance from use.
 *
 *  Instance is not always released instantaeously. If not released, instance
 *  is marked as unused, and will be issued to new active connections.
 *
 *  @return ActiveConnection instance.
 */
void destroy_active_connection(ConnectionManager *cm, ActiveConnection *ac);

/** @brief Destroys ConnectionManager instance.
 *
 *  If ac or cm are NULL, nothing is done. All tracked ActiveConnection
 *  instances are released from memory, including data fields.
 *
 *  @param cm : ActiveConnection instance.
 */
void destroy_connection_manager(ConnectionManager *cm);

#endif //ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_CONNECTION_H
