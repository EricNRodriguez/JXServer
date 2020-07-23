#ifndef ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_REQUEST_H
#define ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_REQUEST_H

#include "../memory/memory.h"

#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#define HEADER_SIZE 1
#define PAYLOAD_LEN_SIZE 8

enum RequestType {
    EchoReq = 0,
    ListDirReq = 2,
    FileSizeReq = 4,
    RetFileReq = 6,
    ShutdownReq = 8
};

typedef struct {
    uint8_t *metadata_buffer;
    size_t metadata_buffer_len;
    size_t metadata_buffer_n;
    uint8_t *payload_buffer;
    uint64_t payload_len;
    size_t payload_buffer_n;
} RequestData;

/** @brief Initialises RequestData instance.
 *
 *  Allocates RequestData instance and sets all appropriate fields.
 *
 *  @return Initialised RequestData instance.
 */
RequestData *init_request_data();

/** @brief Asynchronous reads request data from socket.
 *
 *  Reads request from file descriptor into buffer. Read is NON BLOCKING.
 *  If socket is closed by client, or fails, -1 is returned. Otherwise, 1 is
 *  returned if entire request has been read, otherwise, 0 is returned.
 *
 *  @param rd : RequestData instance.
 *  @param fd : File descriptor to read from.
 *  @return status, -1 (error), 0 (unfinished), 1 (finished).
 */
int request_read(RequestData *rd, int fd);

/** @brief Releases RequestData instance.
 *
 *  RequestData instance and all dynamically allocated fields are released. If
 *  rd is NULL, nothing is done.
 *
 *  @param rd : RequestData instance.
 */
void destroy_reading_data(RequestData *rd);

#endif //ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_REQUEST_H
