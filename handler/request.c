#include "request.h"

RequestData *init_request_data() {
    RequestData *rd = safe_malloc(sizeof(RequestData));

    // alloc metadata buffer
    rd->metadata_buffer = safe_malloc(HEADER_SIZE + PAYLOAD_LEN_SIZE);
    rd->metadata_buffer_len = HEADER_SIZE + PAYLOAD_LEN_SIZE;
    rd->metadata_buffer_n = 0;

    // set payload to null
    rd->payload_buffer = NULL;
    rd->payload_len = 0;
    rd->payload_buffer_n = 0;
    return rd;
}

int request_read(RequestData *rd, int fd) {
    ssize_t n = 0;
    // read header and payload len
    if (rd->metadata_buffer_n < rd->metadata_buffer_len) {
        errno = 0;
        ssize_t n = read(fd, rd->metadata_buffer + rd->metadata_buffer_n,
                rd->metadata_buffer_len - rd->metadata_buffer_n);
        if (n <= 0 && errno != EWOULDBLOCK) {
            return -1;
        }
        rd->metadata_buffer_n += n;

        // initialise payload buffer if metadata has been read
        if (rd->metadata_buffer_n == rd->metadata_buffer_len) {
            for (size_t i = 0; i < PAYLOAD_LEN_SIZE; ++i) {
                rd->payload_len |= rd->metadata_buffer[1 + i] <<
                        ((PAYLOAD_LEN_SIZE - 1 - i) * 8);
            }
            // allocate payload buffer
            rd->payload_buffer = safe_malloc(rd->payload_len);
        }

    } else if (rd->payload_buffer_n < rd->payload_len) {
        errno = 0;
        n = read(fd, rd->payload_buffer + rd->payload_buffer_n,
                rd->payload_len - rd->payload_buffer_n);
        if (n <= 0 && errno != EWOULDBLOCK) {
            return -1;
        }
        rd->payload_buffer_n += n;
    }

    return rd->metadata_buffer_n == rd->metadata_buffer_len &&
           rd->payload_buffer_n == rd->payload_len;
}

bool request_check_complete(RequestData *rd) {
    if (!rd) {
        return false;
    }

    return rd->metadata_buffer_n == rd->metadata_buffer_len &&
            rd->payload_buffer_n == rd->payload_len;
}

void destroy_reading_data(RequestData *rd) {
    if (!rd) {
        return;
    }

    free(rd->metadata_buffer);
    if (rd->payload_buffer) {
        free(rd->payload_buffer);
    }
    free(rd);
    return;
}