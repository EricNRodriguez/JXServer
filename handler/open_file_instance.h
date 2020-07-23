#ifndef ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_OPEN_FILE_INSTANCE_H
#define ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_OPEN_FILE_INSTANCE_H

#include "../memory/memory.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#define UNCLAIMED_WRITE_BUFF_LEN 1024
#define OPEN_FILE_INSTANCES_INIT_LEN 10

typedef struct {
    uint32_t session_id;
    uint64_t offset;
    uint64_t n_requested;
    uint64_t n_read;
    char *file_path;
    FILE *file;
    int reference_count;
    pthread_mutex_t lock;
} OpenFileInstance;

typedef struct {
    OpenFileInstance **open_file_instances;
    size_t n_instances;
    size_t instances_len;
    pthread_mutex_t lock;
} OpenFileInstances;

/** @brief Initialises OpenFileInstances instance.
 *
 *  ofis is set to address of OpenFileInstances instance. Instance is
 *  dynamically allocated, and instances length is set to
 *  OPEN_FILE_INSTANCES_INIT_LEN. If ofis is NULL, nothing is done and
 *  -1 is returned.
 *
 *  @param ofis : Address to store OpenFileInstance pointer.
 *  @return status, -1 on error, 0 otherwise.
 */
int init_open_file_instances(OpenFileInstances **ofis);

/** @brief Initialises OpenFileInstance instance.
 *
 *  ofi is set to address of new OpenFileInstance instance. Appropriate fields
 *  are set, file path is copied. If file path is null,
 *  file doesnt exist, or ofi is NULL, nothing is done and -1 is returned.
 *
 *  @param ofi : Address to store OpenFileInstance pointer.
 *  @param file_path : path to target file.
 *  @param session_id : 4 byte session id.
 *  @param offset : byte offset to begin reading from.
 *  @param n_requested : number of bytes requested from file.
 *  @return status, -1 on error, 0 otherwise.
 */
int init_open_file_instance(OpenFileInstance **ofi, char *file_path,
        uint32_t session_id, uint64_t offset, uint64_t n_requested);

/** @brief Opens a new file as OpenFileInstance in OpenFileInstances param.
 *
 *  Opens a new file as an OpenFileInstance, stored in OpenFileInstances
 *  array. -1 is returned if ofis or file path is NULL, file doesnt exist (or
 *  access is not allowed), or (session_id, file_path, offset, n_requested) tuple
 *  is invalid.
 *
 *  @param ofi : Address to store OpenFileInstance pointer.
 *  @param ofis : OpenFileInstances instance to track new open file instance.
 *  @param file_path : path to requested file.
 *  @param session_id : 4 byte session id.
 *  @param offset : byte offset.
 *  @param n_requested : number of bytes requested from file.
 *  @return status, -1 on error, 0 otherwise.
 */
int open_file(OpenFileInstance **ofi, OpenFileInstances *ofis, char *file_path,
        uint32_t session_id, uint64_t offset, uint64_t n_requested);

/** @brief Destroys open file instance.
 *
 *  Releases all dynamically allocated memory, including fields.
 */
void destroy_open_file_instance(OpenFileInstance *ofi);

/** @brief Destroys open file instances.
 *
 *  Releases all dynamically allocated memory, including fields. All open
 *  file instances are also destroyed.
 */
void destroy_open_file_instances(OpenFileInstances *ofis);

#endif //ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_OPEN_FILE_INSTANCE_H
