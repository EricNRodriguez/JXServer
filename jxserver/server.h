#ifndef COMP2017_ASSIGNMENT_3_SERVER_H
#define COMP2017_ASSIGNMENT_3_SERVER_H

#include "../config/config.h"
#include "../handler/handler.h"
#include "../handler/open_file_instance.h"
#include "../data_structures/compression_dictionary/compression_dict.h"
#include "../data_structures/decompression_tree/decompression_tree.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <sys/sysinfo.h>

struct cleanup_server_thread_args {
    Handler **handlers;
    pthread_t *handler_threads;
    size_t n_handlers;
    Config *config;
    CompressionSegment *comp_dict;
    DecompressionTreeNode *decomp_tree;
    OpenFileInstances *open_file_instances;
    int server_socket_fd;
};

/** @brief Opens socket and listens to the provided address.
 *
 *  If config, comp_dict, or decomp_tree are NULL, nothing is done. A TCP socket
 *  with IPV4 addressing is created. If error occurs, error is printed and program
 *  exits with status EXIT_FAILURE.
 *
 *  Socket is binded to IP and port provided in config. Address is reused.
 *  If bind or listen fail, error is printed to stdout and program exits with
 *  status EXIT_FAILURE. New connections are routed to one of the handler threads.
 *
 *  All arguments are owned by the function, and will be released when a shutdown
 *  request is received.
 *
 *  @param config : server configuration params.
 *  @param comp_dict : compression dictionary.
 *  @param decomp_tree : decompression tree.
 */
void listen_and_serve(Config *config, CompressionSegment *comp_dict,
        DecompressionTreeNode *decomp_tree);

/** @brief Releases all memory provided to and allocated by listen and serve.
 *
 *  Destroys config, comp_dict, and decomp_tree provided to listen and serve.
 *  All handler threads are closed and cleaned. Any open connections are closed.
 *
 *  Intended for use with pthread_cleanup methods.
 *
 *  @param arg : server data, address of struct cleanup_server_thread_args object.
 */
void cleanup_server_thread(void *arg);

#endif //COMP2017_ASSIGNMENT_3_SERVER_H
