#include "server.h"

/** @brief Initialises handler threads.
 *
 *  Creates and initialises handler threads. Number of threads created is
 *  one less than the number of processors available.
 *
 *  handlers argument is initialised, with all initialised handler structs
 *  being stored in the resulting array. handler_threads arg is similarly
 *  initialised, with all threads being stored in the resulting array. n_handlers
 *  argument is initialised to the total number of handler threads created.
 *
 *  If any handler fails to initialise, error message is printed and program
 *  exits with status EXIT_FAILURE.
 *
 *  Handler threads are NOT detached.
 *
 *  @param open_file_instances : OpenFileInstances object.
 *  @param handlers : Address to initialise handlers array.
 *  @param handler_threads : Address to initialise handler_threads array.
 *  @param n_handlers : Address to store number of handler threads created.
 *  @param comp_dict : Compression dictionary.
 *  @param decomp_tree : Decompression tree.
 *  @param config : Server configuration parameters.
 */
static void init_handlers(OpenFileInstances *open_file_instances,
                          Handler ***handlers, pthread_t **handler_threads,
                          size_t *n_handlers, CompressionSegment *comp_dict,
                          DecompressionTreeNode *decomp_tree, Config *config);

static void init_handlers(OpenFileInstances *open_file_instances,
                          Handler ***handlers, pthread_t **handler_threads,
                          size_t *n_handlers, CompressionSegment *comp_dict,
                          DecompressionTreeNode *decomp_tree, Config *config) {

    // allocate return arrays
    *n_handlers = get_nprocs() - 1;
    *handlers = safe_malloc(sizeof(Handler *) * *n_handlers);
    *handler_threads = safe_malloc(sizeof(pthread_t) * *n_handlers);

    // init handler threads
    for (size_t i = 0; i < *n_handlers; ++i) {
        if (init_handler(&(*handlers)[i]) < 0) {
            printf("unable to initialise handler!\n");
            exit(EXIT_FAILURE);
        }
        struct handle_connections_args *args =
                safe_malloc(sizeof(struct handle_connections_args));
        args->h = (*handlers)[i];
        args->ofis = open_file_instances;
        args->main_thread = pthread_self();
        args->comp_dict = comp_dict;
        args->decom_tree = decomp_tree;
        args->config = config;

        pthread_create(&(*handler_threads)[i], NULL, handle_connections, args);
    }
    return;
}

void listen_and_serve(Config *config, CompressionSegment *comp_dict,
                      DecompressionTreeNode *decomp_tree) {
    if (!config || !comp_dict || !decomp_tree) {
        printf("listen and serve failed | config is NULL\n");
        exit(EXIT_FAILURE);
    }

    int server_sock_fd = -1;
    server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock_fd < 0) {
        perror("error opening socket\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    // ipv4
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = config->port;
    // host ip
    server_addr.sin_addr = config->ip_addr;

    int option = 1;
    // reuse address and port
    if (setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &option, sizeof(option)) < 0) {
        perror("error setting socket options");
        exit(EXIT_FAILURE);
    }

    // bind file descriptor to the port
    if (bind(server_sock_fd, (struct sockaddr *) &server_addr,
              sizeof(server_addr)) < 0) {
        perror("error binding socket\n");
        exit(EXIT_FAILURE);
    }

    // queue max number of connections
    if (listen(server_sock_fd, SOMAXCONN) < 0) {
        perror("error listening");
        exit(EXIT_FAILURE);
    }

    // initialise shared open file instances memory
    OpenFileInstances *open_file_instances = safe_malloc(sizeof(OpenFileInstances));
    init_open_file_instances(&open_file_instances);

    // init handler threads
    Handler **handlers = NULL;
    pthread_t *handler_threads = NULL;
    size_t n_handlers = 0;
    init_handlers(open_file_instances, &handlers, &handler_threads, &n_handlers,
                  comp_dict, decomp_tree, config);

    // init cleanup on thread termination
    struct cleanup_server_thread_args args = {
            .handlers = handlers,
            .handler_threads = handler_threads,
            .n_handlers = n_handlers,
            .config = config,
            .comp_dict = comp_dict,
            .decomp_tree = decomp_tree,
            .server_socket_fd = server_sock_fd,
            .open_file_instances = open_file_instances
    };
    pthread_cleanup_push(cleanup_server_thread, &args);

    // accept new connections
    uint32_t addr_len = sizeof(struct sockaddr_in);
    size_t thread_index = 0;
    while (1) {
        int client_sock_fd = accept(server_sock_fd,
                                    (struct sockaddr *) &server_addr, &addr_len);
        if (client_sock_fd >= 0) {
            // non blocking io
            fcntl(client_sock_fd, F_SETFL, fcntl(client_sock_fd, F_GETFL, 0)|O_NONBLOCK);
            // create req
            if (new_client(handlers[thread_index], client_sock_fd) < 0) {
                break;
            }
        } else {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }
        // round robbin
        thread_index = thread_index % n_handlers;
    }

    // reap handler threads
    pthread_cleanup_pop(1);
    exit(0);
    return;
}

void cleanup_server_thread(void *arg) {
    struct cleanup_server_thread_args *args =
            (struct cleanup_server_thread_args *) arg;

    // cancel handler threads
    for (size_t i = 0; i < args->n_handlers; ++i) {
        pthread_cancel(args->handler_threads[i]);
    }
    // reap
    for (size_t i = 0; i < args->n_handlers; ++i) {
        pthread_join(args->handler_threads[i], NULL);
    }

    destroy_config(args->config);
    destroy_decompression_tree(args->decomp_tree);
    destroy_compression_dict(args->comp_dict);
    destroy_open_file_instances(args->open_file_instances);

    close(args->server_socket_fd);

    free(args->handlers);
    free(args->handler_threads);
    return;
}