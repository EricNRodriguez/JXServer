#ifndef ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_HANDLER_H
#define ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_HANDLER_H

#include "connection.h"
#include "responce.h"
#include "header_masks.h"
#include "open_file_instance.h"
#include "../config/config.h"
#include "../data_structures/compression_dictionary/compression_dict.h"
#include "../data_structures/decompression_tree/decompression_tree.h"

#include <stdint.h>
#include <stdatomic.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <signal.h>

#define EPOLL_EVENTS_SIZE_INIT 1000

typedef struct {
    atomic_size_t n_connections;
    int epoll_fd;
    struct epoll_event *events;
    size_t events_len;
    ConnectionManager *conn_manager;
} Handler;

struct handle_connections_args {
    Handler *h;
    OpenFileInstances *ofis;
    pthread_t main_thread;
    CompressionSegment *comp_dict;
    DecompressionTreeNode *decom_tree;
    Config *config;
};

/** @brief Initialises handler instance.
 *
 *  Allocates provided address to handler instance. Handler fields are
 *  allocated and initialised to their default values.
 *
 *  @param h : Address of handler pointer.
 */
int init_handler(Handler **h);

/** @brief Adds connection to those watched by the handler.
 *
 *  Adds the new connections file descriptor to the handlers epoll queue.
 *  New active connection instance is attached, with event set to EPOLLIN.
 *  If successful, handlers n_connections field is incremented, otherwise, -1
 *  is returned.
 *
 *  @param h : Handler instance.
 *  @param client_sock_fd : Open file descriptor of new client.
 */
int new_client(Handler *h, int client_sock_fd);

/** @brief Begins handling requests.
 *
 *  Starts handler. Requests will be handled indefinitly once called, only
 *  stopping when a shutdown request is recieved.
 *
 *  @param arg : handle_connections_args instance.
 */
void *handle_connections(void *arg);

/** @brief Releases all memory provided to and allocated by handler thread.
 *
 *  All active connections are closed, and associated data also destroyed.
 *  Handler is also destroyed.
 *
 *  Function is intended to be used by pthread_cleanup_push.
 *
 *  @param arg : address of handler.
 */
void cleanup_handler(void *arg);

#endif //ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_HANDLER_H