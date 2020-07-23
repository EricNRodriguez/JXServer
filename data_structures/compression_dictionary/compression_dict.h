#ifndef ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_COMPRESSION_DICT_H
#define ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_COMPRESSION_DICT_H

#include "../../memory/memory.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define COMPRESSION_DICT_LEN 256
#define COMPRESSION_DICT_FILE_NAME "compression.dict"
#define LSB_MASK 0x01

typedef struct {
    uint8_t uncompressed;
    uint8_t compressed_len; // num of bits
    uint32_t compressed; // left padded
} CompressionSegment;

/** @brief Reads dictionary from binary.
 *
 *  Reads decompression dictionary from current directory. It is assumed
 *  that the file is named compression.dict. If file doesnt exist, or program
 *  does not have permissions, or reading fails, error message is printed and
 *  program exits with status EXIT_FAILURE.
 *
 *  RETURNED ARRAY ALWAYS HAS A LENGTH OF 256.
 *
 *  @return Compression dictionary address.
 */
CompressionSegment *parse_compression_dictionary();

/** @brief Destroys compression dictionary instance.
 *
 *  All dynamicallt allocated segments and associated fields are released.
 *
 *  @param dict : Compression dictionary to be released.
 */
void destroy_compression_dict(CompressionSegment *dict);

#endif //ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_COMPRESSION_DICT_H
