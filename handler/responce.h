#ifndef ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_RESPONCE_H
#define ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_RESPONCE_H

#include "../memory/memory.h"
#include "request.h"
#include "open_file_instance.h"
#include "../data_structures/compression_dictionary/compression_dict.h"
#include "../data_structures/decompression_tree/decompression_tree.h"
#include "../data_structures/bit_vector/bit_vector.h"

#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

#define INIT_LIST_FILES_BUFF_SIZE 64
#define INIT_RET_FILE_BUFF_SIZE 512
#define INIT_DECOMPRESSED_PAYLOAD_LEN 64
#define ARRAY_GROWTH_RATE 2

enum ResponceType {
    EchoRsp = 1,
    ListDirRsp = 3,
    FileSizeRsp = 5,
    RetFileRsp = 7,
    Error = 15
};

typedef struct {
    enum ResponceType type;
    uint8_t *write_buffer;
    size_t write_buffer_len;
    size_t write_n;
    size_t n_written;
    void *ptr;
} ResponceData;

/** @brief Initialises Response Data Object.
 *
 *  Allocates a response data object, and sets the corresponding fields
 *  based on the arguments provided. If provided write buffer is NULL,
 *  nothing is done and NULL is returned.
 *
 *  @param type : Type of responce (Echo, RetFile etc).
 *  @param write_buffer : Array of data to be sent.
 *  @param write_buffer_len : Length of write buffer.
 *  @param ptr : Optional data to be attached. This is exclusively used for
 *  retrieving files, where an OpenFileInstance is attached.
 *  @return ResponseData object.
 */
ResponceData *init_responce_data(enum ResponceType type, uint8_t *write_buffer,
        size_t write_buffer_len, void *ptr);

/** @brief Asynchronous write from buffer to file descriptor.
 *
 *  Writes from buffer to file descriptor. Writes are NON BLOCKING. If provided
 *  responce data is NULL, nothing is done and -1 is returned. If
 *  0 or less bytes are written, and errno is not EWOULDBLOCK, -1 is returned.
 *  If no errors occur, 1 is returned if all bytes in the write buffer have
 *  been written, otherwise, 0 is returned.
 *
 *  @param rd : ResponceData instance.
 *  @param fd : File descriptor to write too.
 *  @return completion status : -1 (error), 0 (unfinished), 1 (finished).
 */
int responce_write(ResponceData *rd, int fd);

/** @brief Deallocates ResponceData instance.
 *
 *  Frees ResponceData instance and all dynamically allocated fields. If
 *  ResponceData is NULL, nothing is done. If ResponceData type is RetFileRsp,
 *  open file instance reference counter is decremented. Write buffer is
 *  deallocated. ResponceData instance is deallocated.
 *
 *  @param rd : ResponceData instance to be deallocated.
 */
void destroy_responce_data(ResponceData *rd);

/** @brief Handles echo request.
 *
 *  Creates a ResponceData instance, with write buffer containing an echo
 *  responce to be written back to the client. Appropriate header, payload
 *  length and compression is handled. If error occurs, NULL is returned.
 *
 *  @param compressed : Compression flag for payload. True if data is compressed.
 *  @param req_compression : Requires Compression flag for responce data.
 *  @param payload : Request payload.
 *  @param payload_len : Length of payload.
 *  @param comp_dict : Compression dictionary (CompressionSegment *) instance.
 *  @return ResponceData instance.
 */
ResponceData *echo(bool compressed, bool req_compression, uint8_t *payload,
        uint64_t payload_len, CompressionSegment *comp_dict);

/** @brief Handles error.
 *
 *  Creates a ResponceData instance for error responce. Write buffer is allocated
 *  and contents set appropriatly. Type is set to Error, and payload length
 *  set to 0. No compression flags are set.
 *
 *  @return ResponceData instance.
 */
ResponceData *error();

/** @brief Handles List Directory Request.
 *
 *  Creates a ResponceData instance for ListDir request. If directory provided
 *  in payload is invalid, error responce is created. Successful request will
 *  contain a responce payload with null terminated file names, seperated by
 *  null bytes. ONLY regular files are returned. All directories and files in
 *  sub directories are not included.
 *
 *  @param compressed : Compression flag for payload. True if data is compressed.
 *  @param req_compression : Requires Compression flag for responce data.
 *  @param payload : Request payload.
 *  @param payload_len : Length of payload.
 *  @param dir : directory path.
 *  @param comp_dict : Compression dictionary (CompressionSegment *) instance.
 *  @return ResponceData instance.
 */
ResponceData *list_files(bool compressed, bool req_compression, uint8_t *payload,
        uint64_t payload_len, char *dir, CompressionSegment *comp_dict);

/** @brief Handles FileSize request.
 *
 *  Creates a ResponceData instance, with write buffer containing size of
 *  requested file. Appropriate header, payload length and compression is handled.
 *  If error occurs (file does not exist), error responce is created instead.
 *
 *  @param compressed : Compression flag for payload. True if data is compressed.
 *  @param req_compression : Requires Compression flag for responce data.
 *  @param payload : Request payload.
 *  @param payload_len : Length of payload.
 *  @param dir : directory path.
 *  @param comp_dict : Compression dictionary (CompressionSegment *) instance.
 *  @param decom_tree : Decompression tree (DecompressionTreeNode *) instance.
 *  @return ResponceData instance.
 */
ResponceData *get_file_size(bool compressed, bool req_compression, uint8_t *payload,
        uint64_t payload_len, char *dir, CompressionSegment *comp_dict,
        DecompressionTreeNode *decom_tree);

/** @brief Handles RetFile request.
 *
 *  Creates a ResponceData instance, with write buffer containing either a
 *  subset or entierty of the requested file contents.
 *
 *  Responce payload is structured as follows:
 *      4 bytes - session ID.
 *      8 bytes - byte offset in file.
 *      8 bytes - number of bytes (from the file) contained.
 *      Variable bytes - file data (can be a subset).
 *
 *  Appropriate header, payload length and compression is handled. Concurrent
 *  requests for the file with the same session ID will have the file multiplexed
 *  across both this request and the existing requests. If session id
 *  is already being used, and the file requested is differet, error occurs.
 *  Concurrent request for a file with different session ids will be treated
 *  independently. If error occurs, error responce is created instead.
 *
 *  @param compressed : Compression flag for payload. True if data is compressed.
 *  @param req_compression : Requires Compression flag for responce data.
 *  @param payload : Request payload.
 *  @param payload_len : Length of payload.
 *  @param dir : directory path.
 *  @param comp_dict : Compression dictionary (CompressionSegment *) instance.
 *  @param decom_tree : Decompression tree (DecompressionTreeNode *) instance.
 *  @param ofis : Current OpenFileInstances (shared between requests).
 *  @return ResponceData instance.
 */
ResponceData *ret_file(bool compressed, bool req_compression, uint8_t *payload,
        uint64_t payload_len, char *dir, CompressionSegment *comp_dict,
        DecompressionTreeNode *decom_tree, OpenFileInstances *ofis);


/** @brief Refills write buffer for RetFile request.
 *
 *  File data is sent over multiple responces for large files. Once a subset
 *  of the data has been sent, this function will refill the write buffer
 *  of the responce instance, for reuse.
 *
 *  If no data is left, -1 is returned. Otherwise, 0 is returned and write buffer
 *  contains data to be transmitted.
 *
 *  @param rd : ResponceData instance.
 *  @param comp_dict : Compression dictionary instance.
 *  @param req_compr : Requires compression payload (if true, write buffer is
 *  compressed).
 */
int ret_file_fill_write_buffer(ResponceData *rd, CompressionSegment *comp_dict,
        bool req_compr);

#endif //ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_RESPONCE_H
