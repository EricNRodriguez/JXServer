#include "handler.h"

/** @brief Handles requests.
 *
 *  Handles requests by routing RequestData to appropriate handler
 *  (echo, error, etc...). If request type is unknown, request will be routed
 *  to error handler. ResponceData instance with loaded write buffer is created
 *  and returned.
 *
 *  @param rd : RequestData instance.
 *  @param fd : client file descriptor.
 *  @param h : Handler instance.
 *  @param config : Server configuration parameters.
 *  @param comp_dict : Compression dictionary.
 *  @param decomp_tree : Decompression tree.
 *  @param ofis : OpenFileInstances.
 *  @param main_thread : main thread id.
 *  @return ResponceData instance.
 */
static ResponceData *handle_request(RequestData *rd, int fd,  Handler *h,
        Config *config, CompressionSegment *comp_dict,
        DecompressionTreeNode *decomp_tree, OpenFileInstances *ofis,
        pthread_t main_thread);

/** @brief Updates active request after read.
 *
 *  Checks if request is ready to be handled. If ready, payload is passed to
 *  the appropriate handler, and the connection is updated to a ResponceData
 *  object. If connection is terminated by user, or socket fails, the
 *  ActiveConnection instance is destroyed.
 *
 *  @param h : Handler instance.
 *  @param conn : ActiveConnection instance.
 *  @param config : Server configuration instance.
 *  @param comp_dict : Compression dictionary.
 *  @param decomp_tree : Decompression tree.
 *  @param ret_read : status of read (-1 (error), 0 (unfinished), 1 (finished)).
 *  @param conn_event_index : index of event in epoll events array.
 *  @param ofis : Server OpenFileInstances instance.
 *  @param main_thread : Server thread.
 */
static void update_request(Handler *h, ActiveConnection *conn, Config *config,
        CompressionSegment *comp_dict, DecompressionTreeNode *decomp_tree,
        int ret_read, size_t conn_event_index, OpenFileInstances *ofis,
        pthread_t main_thread);

/** @brief Updates responce post-write.
 *
 *  Updates responce based on status returned from write. If negative, connection
 *  is terminated. If zero, nothing is done (responce not entierly sent). If 1,
 *  responce is handled appropriatly:
 *
 *      Error      -> connection terminated.
 *      RetFileRsp -> recycled if file has completely sent, reloads payload
 *                    with more data otherwise.
 *      Default    -> Recycles connection (new request).
 *
 *  @param h : Handler instance.
 *  @param conn : ActiveConnection instance.
 *  @param comp_dict : Compression dictionary.
 *  @param decomp_tree : Decompression tree.
 *  @param ret_write : status of write (-1 (error), 0 (unfinished), 1 (finished)).
 *  @param conn_event_index : index of event in epoll events array.
 */
static void update_responce(Handler *h, ActiveConnection *conn,
        CompressionSegment *comp_dict, DecompressionTreeNode *decomp_tree,
        int ret_write, size_t conn_event_index);

/** @brief Recycles connection.
 *
 *  Updates connection status to Request, and connection data is
 *  updated to a new RequestData instance. EPOLL event updated to
 *  EPOLLIN.
 *
 *  @param h : Handler instance.
 *  @param conn : ActiveConnection instance.
 *  @param conn_event_index : index of event in epoll events array.
 *  @param config : Server configuration.
 *  @param comp_dict : Compression dictionary.
 *  @param decomp_tree : Decompression tree.
 *  @param ofis : Server OpenFileInstances instances.
 *  @param main_thread : Server thread.
 */
static void recycle_connection(Handler *h, ActiveConnection *conn,
        size_t conn_event_index, Config *config, CompressionSegment *comp_dict,
        DecompressionTreeNode *decomp_tree, OpenFileInstances *ofis,
        pthread_t main_thread);

/** @brief Terminates connection.
 *
 *  Terminates connection. All allocated memory is released. Socket is closed.
 *
 *  @param h : Handler instance.
 *  @param conn : ActiveConnection instance.
 *  @param conn_event_index : index of event in epoll events array.
 */
static void terminate_connection(Handler *h, ActiveConnection *conn,
        size_t conn_event_index);

int init_handler(Handler **h) {
    if (!h) {
        return -1;
    }

    *h = safe_malloc(sizeof(Handler));
    (*h)->epoll_fd = epoll_create1(0);
    (*h)->events = safe_malloc(sizeof(struct epoll_event) * EPOLL_EVENTS_SIZE_INIT);
    (*h)->events_len = EPOLL_EVENTS_SIZE_INIT;
    (*h)->conn_manager = init_connection_manager();
    atomic_init(&(*h)->n_connections, 0);
    return 0;
}

int new_client(Handler *h, int client_sock_fd) {
    if (!h) {
        return -1;
    }

    static struct epoll_event ev;
    ev.events = EPOLLIN; // read
    ActiveConnection *ac = new_active_connection(h->conn_manager, client_sock_fd, Request);
    ev.data.ptr = (void *) ac;
    // watch file descriptor
    if (epoll_ctl(h->epoll_fd, EPOLL_CTL_ADD, client_sock_fd, &ev) < 0) {
        destroy_active_connection(h->conn_manager, ac);
        return -1;
    }

    // record new connection
    atomic_fetch_add(&h->n_connections, 1);
    return 0;
}

static void terminate_connection(Handler *h, ActiveConnection *conn,
        size_t conn_event_index) {
    epoll_ctl(h->epoll_fd, EPOLL_CTL_DEL, conn->fd, &h->events[conn_event_index]);
    atomic_fetch_sub(&h->n_connections, 1);
    destroy_active_connection(h->conn_manager, conn);

    return;
}

static void recycle_connection(Handler *h, ActiveConnection *conn,
        size_t conn_event_index, Config *config, CompressionSegment *comp_dict,
        DecompressionTreeNode *decomp_tree, OpenFileInstances *ofis,
        pthread_t main_thread) {

    if (conn->stat == Responce) {
        destroy_responce_data(conn->data);
        epoll_ctl(h->epoll_fd, EPOLL_CTL_DEL, conn->fd,
                  &h->events[conn_event_index]);
        // update connection status
        conn->stat = Request;
        conn->data = init_request_data();
        // create new event
        struct epoll_event ev;
        ev.events = EPOLLIN; // read
        ev.data.ptr = conn;
        if (epoll_ctl(h->epoll_fd, EPOLL_CTL_ADD, conn->fd, &ev) < 0) {
            printf("epoll failed!\n");
            exit(EXIT_FAILURE);
        }
    } else {
        ResponceData *rd = handle_request((RequestData *) conn->data, conn->fd,
                                          h, config, comp_dict, decomp_tree,
                                          ofis, main_thread);
        destroy_reading_data((RequestData *) conn->data);
        conn->stat = Responce;
        conn->data = rd;
        // delete old event
        epoll_ctl(h->epoll_fd, EPOLL_CTL_DEL, conn->fd,
                  &h->events[conn_event_index]);
        // create new event
        struct epoll_event ev;
        ev.events = EPOLLOUT; // read
        ev.data.ptr = conn;
        if (epoll_ctl(h->epoll_fd, EPOLL_CTL_ADD, conn->fd, &ev) < 0) {
            // finish up
            printf("epoll failed!\n");
            exit(EXIT_FAILURE);
        }
    }
    return;
}

static void update_responce(Handler *h, ActiveConnection *conn,
        CompressionSegment *comp_dict, DecompressionTreeNode *decomp_tree,
        int ret_write, size_t conn_event_index) {

    // connection closed
    if (ret_write < 0) {
        terminate_connection(h, conn, conn_event_index);
        return;
    }
    // responce not finished sending
    if (ret_write == 0) {
        return;
    }

    // responce finished sending
    ResponceData *rd = (ResponceData *) conn->data;
    if (rd->type == Error) {
        // close connection after error
        terminate_connection(h, conn, conn_event_index);
    } else if (rd->type == RetFileRsp) {
        OpenFileInstance *ofi = (OpenFileInstance *) rd->ptr;
        pthread_mutex_lock(&ofi->lock);
        // file completely sent
        if (rd->n_written == rd->write_n && ofi->n_read == ofi->n_requested)  {
            pthread_mutex_unlock(&ofi->lock);
            recycle_connection(h, conn, conn_event_index, NULL, NULL, NULL, NULL,
                    0);
        // send another packet
        } else {
            pthread_mutex_unlock(&ofi->lock);
            // refill write buffer
            ret_file_fill_write_buffer(rd, comp_dict, rd->write_buffer[0] &
                    MSG_HEADER_REQ_COMPRESSION_MASK);
        }
    } else {
        // new request
        recycle_connection(h, conn, conn_event_index, NULL, NULL, NULL, NULL, 0);
    }
    return;
}

static void update_request(Handler *h, ActiveConnection *conn, Config *config,
        CompressionSegment *comp_dict, DecompressionTreeNode *decomp_tree,
        int ret_read, size_t conn_event_index, OpenFileInstances *ofis,
        pthread_t main_thread) {

    if (ret_read < 0) {
        terminate_connection(h, conn, conn_event_index);
    } else if (ret_read == 1) {
        recycle_connection(h, conn, conn_event_index, config, comp_dict,
                decomp_tree, ofis, main_thread);
    }
    return;
}

void *handle_connections(void *arg) {
    if (!arg) {
        return NULL;
    }

    signal(SIGPIPE, SIG_IGN);

    struct handle_connections_args *args = (struct handle_connections_args *) arg;
    Handler *h = args->h;
    OpenFileInstances *ofis = args->ofis;
    pthread_t main_thread = args->main_thread;
    CompressionSegment *comp_dict = args->comp_dict;
    DecompressionTreeNode *decomp_tree = args->decom_tree;
    Config *config = args->config;
    free(args);

    // init cleanup on thread exit
    pthread_cleanup_push(cleanup_handler, h);

    int fds;
    ActiveConnection *conn = NULL;
    while ((fds = epoll_wait(h->epoll_fd, h->events, h->events_len, -1))) {
        for (int i = 0; i < fds; ++i) {
            conn = (ActiveConnection *) h->events[i].data.ptr;
            // read ready
            if (h->events[i].events & EPOLLIN && conn->stat == Request) {
                int ret_read = request_read((RequestData *)conn->data, conn->fd);
                update_request(h, conn, config, comp_dict, decomp_tree ,ret_read,
                               i, ofis, main_thread);
            } else if (h->events[i].events & EPOLLOUT && conn->stat == Responce) {
                int ret_write = responce_write((ResponceData *) conn->data,
                                               conn->fd);
                update_responce(h, conn, comp_dict, decomp_tree, ret_write, i);
            }
        }
        // resize if necessary
        if (atomic_load(&h->n_connections) >= h->events_len) {
            h->events = safe_realloc(h->events, h->events_len * ARRAY_GROWTH_RATE);
            h->events_len *= ARRAY_GROWTH_RATE;
        }
    }

    // execute cleanup
    pthread_cleanup_pop(1);
    return NULL;
}

static ResponceData *handle_request(RequestData *rd, int fd,  Handler *h, Config *config,
                             CompressionSegment *comp_dict,
                             DecompressionTreeNode *decomp_tree,
                             OpenFileInstances *ofis, pthread_t main_thread) {

    if (!rd || !comp_dict || !decomp_tree || !config->dir || !ofis) {
        return NULL;
    }

    uint8_t header = rd->metadata_buffer[0];
    enum RequestType req_type = (header & MSG_HEADER_TYPE_MASK) >> 4;
    bool compressed_payload = (header & MSG_HEADER_COMPRESSION_MASK) >> 3;
    bool requires_compression = (header & MSG_HEADER_REQ_COMPRESSION_MASK) >> 2;

    ResponceData *ret = NULL;
    switch (req_type) {
        case EchoReq:
            ret = echo(compressed_payload, requires_compression, rd->payload_buffer,
                       rd->payload_len, comp_dict);
            break;
        case ListDirReq:
            ret = list_files(compressed_payload, requires_compression,
                             rd->payload_buffer, rd->payload_len, config->dir,
                             comp_dict);
            break;
        case FileSizeReq:
            ret = get_file_size(compressed_payload, requires_compression,
                                rd->payload_buffer, rd->payload_len, config->dir,
                                comp_dict, decomp_tree);
            break;
        case RetFileReq:
            ret = ret_file(compressed_payload, requires_compression,
                           rd->payload_buffer, rd->payload_len, config->dir,
                           comp_dict, decomp_tree, ofis);
            break;
        case ShutdownReq:
            // shutdown server
            pthread_cancel(main_thread);
            break;
        default:
            ret = error();
            break;
    }
    return ret;
}

void cleanup_handler(void *arg) {
    if (!arg) {
        return;
    }

    Handler *h = (Handler *) arg;
    destroy_connection_manager(h->conn_manager);
    close(h->epoll_fd);
    free(h->events);
    free(h);
    return;
}