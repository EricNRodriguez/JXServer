#include "responce.h"

/** @brief Adds header and payload length to beginning of buffer.
 *
 *  Formats header and copies formatted byte to front of buffer. Payload
 *  length is also coppied to bytes 1-9.
 *
 *  @param dest : buffer to be written too.
 *  @param rt : Responce Type (EchoRsp, ListDirRep etc).
 *  @param compressed_payload : Compressed Payload Flag.
 *  @param payload_len : Length of payload.
 */
static void write_metadata(uint8_t *dest, enum ResponceType rt,
        bool compressed_payload, uint64_t payload_len);

/** @brief Checks if file is regular.
 *
 *  File given by <dir>/<name> is checked.
 *
 *  @param dir : Target directory path.
 *  @param name : Target file name.
 *  @return boolean, True if regular, False otherwise.
 */
static bool is_regular_file(char *dir, char *name);

/** @brief Compresses data.
 *
 *  Compresses payload using provided compression dictionary. Resulting
 *  compressed data is stored in allocated array, and dest is set to the starting
 *  address. dest_size is set as the size of the allocated array.
 *
 *  If any parameters are NULL, nothing is done and -1 is returned (error).
 *  Otherwise, data is compressed and 0 is returned.
 *
 *  @param comp_dict : Compression dictionary.
 *  @param uncomp_payload : Uncompressed payload.
 *  @param payload_size : size of uncompressed payload.
 *  @param dest : points to compressed array address on success.
 *  @param dest_size : set to size of compressed array on success.
 *  @param write_offset : data to the right of this address will be compressed.
 *                        This is used to easily exclude data at the beginning of
 *                        the uncompressed payload from compression.
 *  @return 0 on success, -1 on error.
 */
static int compress(CompressionSegment *comp_dict, uint8_t *uncomp_payload,
        uint64_t payload_size, uint8_t **dest, uint64_t *dest_size,
        size_t write_offset);

/** @brief Decompresses data.
 *
 *  Decompresses compressed payload using provided decompression tree.
 *  Resulting decompressed data is allocated and pointed to by dest param.
 *  dest_size is set to allocated length.
 *
 *  If any parameters are NULL, nothing is done and -1 is returned (error).
 *  Otherwise, data is decompressed and 0 is returned.
 *
 *
 *  @param decom_tree :  Decompression tree root node.
 *  @param compr_payload : Payload to be decompressed.
 *  @param compr_payload_n : number of bytes to be decompressed.
 *  @param decompressed_payload : points to decompressed array on success.
 *  @param decompressed_payload_n : set to decompressed array len on success.
 *  @return 0 on success, -1 on error.
 */
static int decompress(DecompressionTreeNode *decom_tree, uint8_t *compr_payload,
        size_t compr_payload_n, uint8_t **decompressed_payload,
        uint64_t *decompressed_payload_n);

ResponceData *init_responce_data(enum ResponceType type, uint8_t *write_buffer,
        size_t write_buffer_len, void *ptr) {

    if (!write_buffer) {
        return NULL;
    }

    ResponceData *rd = safe_malloc(sizeof(ResponceData));
    rd->type = type;
    rd->write_buffer = write_buffer;
    rd->write_n = write_buffer_len; // full buffer
    rd->write_buffer_len = write_buffer_len;
    rd->n_written = 0;
    rd->ptr = ptr;

    return rd;
}

int responce_write(ResponceData *rd, int fd) {
    if (!rd) {
        return -1;
    }

    // async write
    if (rd->n_written < rd->write_n) {
        errno = 0;
        ssize_t n = write(fd, rd->write_buffer + rd->n_written,
                rd->write_n - rd->n_written);
        // socket closed by client, or failed
        if (n <= 0 && errno != EWOULDBLOCK) {
            return -1;
        }
        rd->n_written += n;
    }

    return rd->n_written == rd->write_n;
}

void destroy_responce_data(ResponceData *rd) {
    if (!rd) {
        return;
    }

    if (rd->type == RetFileRsp) {
        OpenFileInstance *ofi = (OpenFileInstance *) rd->ptr;
        pthread_mutex_lock(&ofi->lock);
        ofi->reference_count--;
        pthread_mutex_unlock(&ofi->lock);
    }

    free(rd->write_buffer);
    free(rd);
    return;
}

static void write_metadata(uint8_t *dest, enum ResponceType rt,
        bool compressed_payload, uint64_t payload_len) {

    // header construction
    uint8_t header = 0x00;
    header |= (rt << 4) | (compressed_payload << 3);
    dest[0] = header;

    // write payload in network byte order
    for (size_t i = 0; i < PAYLOAD_LEN_SIZE; ++i) {
        dest[1+i] = (payload_len >> ((PAYLOAD_LEN_SIZE - 1 - i) * 8)) & 0xFF;
    }
    return;
}

ResponceData *error() {
    uint8_t *write_buff = safe_malloc(HEADER_SIZE + PAYLOAD_LEN_SIZE);
    // init buffer
    write_metadata(write_buff, Error, false, 0);
    ResponceData *ret = init_responce_data(Error, write_buff,
            HEADER_SIZE + PAYLOAD_LEN_SIZE, NULL);
    return ret;
}

ResponceData *echo(bool compressed, bool req_compression, uint8_t *payload,
        uint64_t payload_len, CompressionSegment *comp_dict) {

    ResponceData *ret = NULL;
    // compressed and requires compression
    if  (!compressed && req_compression) {
        size_t len = 0;
        uint8_t *compressed_data = NULL;
        compress(comp_dict, payload, payload_len, &compressed_data, &len,
                HEADER_SIZE + PAYLOAD_LEN_SIZE);
        write_metadata(compressed_data, EchoRsp, true,
                len - HEADER_SIZE - PAYLOAD_LEN_SIZE);
        ret = init_responce_data(EchoRsp, compressed_data, len, NULL);
    } else {
        // send back what you got
        uint8_t *write_data =
                safe_malloc(payload_len + HEADER_SIZE + PAYLOAD_LEN_SIZE);
        write_metadata(write_data, EchoRsp, compressed, payload_len);
        // copy in remaining payload
        memcpy(write_data + HEADER_SIZE + PAYLOAD_LEN_SIZE, payload, payload_len);
        // initialise responce
        ret = init_responce_data(EchoRsp, write_data,
                payload_len + HEADER_SIZE + PAYLOAD_LEN_SIZE, NULL);
    }

    return ret;
}

static bool is_regular_file(char *dir, char *name) {
    char *path = safe_malloc(strlen(name) + strlen(dir) + 2);
    sprintf(path, "%s/%s", dir, name);
    struct stat path_stat;
    if (stat(path, &path_stat) < 0) {
        return false;
    }
    free(path);
    return S_ISREG(path_stat.st_mode);
}

ResponceData *list_files(bool compressed, bool req_compression, uint8_t *payload,
        uint64_t payload_len, char *dir, CompressionSegment *comp_dict) {

    // payload should be empty
    if (payload_len) {
        return error();
    }

    uint8_t *write_buff = safe_malloc(INIT_LIST_FILES_BUFF_SIZE + HEADER_SIZE +
            PAYLOAD_LEN_SIZE);
    size_t write_buff_len = INIT_LIST_FILES_BUFF_SIZE;
    uint64_t write_buff_n = HEADER_SIZE + PAYLOAD_LEN_SIZE;

    DIR *d = NULL;
    struct dirent *ent = NULL;
    if ((d = opendir(dir))) {
        while ((ent = readdir(d))) {
            // file name length
            size_t name_len = strchr(ent->d_name, 256) - ent->d_name;
            // resize payload if necessary
            if (write_buff_n + name_len > write_buff_len - 1) {
                write_buff = safe_realloc(write_buff,
                        ARRAY_GROWTH_RATE * write_buff_len);
                write_buff_len *= ARRAY_GROWTH_RATE;
            }
            if (is_regular_file(dir, ent->d_name)) {
                // write file name to directory
                memcpy(write_buff + write_buff_n, ent->d_name, name_len);
                write_buff_n += name_len;
                write_buff[write_buff_n] = 0;
                write_buff_n++;
            }
        }
        closedir(d);
    } else {
        free(write_buff);
        return error();

    }

    ResponceData *ret = NULL;
    if (!req_compression) {
        // write header and payload len
        write_metadata(write_buff, ListDirRsp, req_compression,
                write_buff_n - HEADER_SIZE - PAYLOAD_LEN_SIZE);
        // responce buffer
        ret = init_responce_data(ListDirRsp, write_buff, write_buff_n, NULL);
    } else {
        size_t len = 0;
        uint8_t *compressed_data = NULL;
        // compress payload
        size_t offset = HEADER_SIZE + PAYLOAD_LEN_SIZE;
        compress(comp_dict, write_buff + offset, write_buff_n - offset,
                &compressed_data, &len, offset);
        free(write_buff);
        // write metadata
        write_metadata(compressed_data, ListDirRsp, true, len - offset);
        // initialise responce
        ret = init_responce_data(EchoRsp, compressed_data, len, NULL);
    }
    return ret;
}

ResponceData *get_file_size(bool compressed, bool req_compression,
        uint8_t *payload, uint64_t payload_len, char *dir,
        CompressionSegment *comp_dict, DecompressionTreeNode *decom_tree) {

    // format file path
    char *file_path = NULL;
    if (!compressed) {
        file_path = safe_malloc(strlen((char *) payload) + strlen(dir) + 2);
        // format file path
        sprintf(file_path, "%s/%s", dir, payload);
    } else {
        // decompress file path
        uint8_t *decompressed_payload = NULL;
        uint64_t decompressed_payload_n = 0;
        decompress(decom_tree, payload, payload_len, &decompressed_payload,
                &decompressed_payload_n);
        // format file path
        file_path = safe_malloc(strlen((char *) payload) + strlen(dir) + 2);
        sprintf(file_path, "%s/%s", dir, decompressed_payload);
    }

    // get file len
    struct stat file_stat;
    if (stat(file_path, &file_stat) < 0) {
        free(file_path);
        return error();
    }
    uint64_t file_size = (uint64_t) file_stat.st_size;
    free(file_path);

    uint8_t *write_buff = NULL;
    size_t write_buff_n = 0;

    ResponceData *ret = NULL;

    if (!req_compression) {
        write_buff = safe_malloc(sizeof(file_size) + HEADER_SIZE +
                PAYLOAD_LEN_SIZE);
        write_buff_n = sizeof(file_size) + HEADER_SIZE + PAYLOAD_LEN_SIZE;
        // write metadata
        write_metadata(write_buff, FileSizeRsp, req_compression,
                sizeof(file_size));
        // convert to network byte order
        file_size = htobe64(file_size);
        memcpy(write_buff + HEADER_SIZE + PAYLOAD_LEN_SIZE, &file_size,
                sizeof(file_size));
        // init responce
        ret = init_responce_data(FileSizeRsp, write_buff, write_buff_n, NULL);
    } else {
        // convert to network byte order
        file_size = htobe64(file_size);
        // compress
        size_t len = 0;
        uint8_t *compressed_data = NULL;
        size_t offset = HEADER_SIZE + PAYLOAD_LEN_SIZE;
        // compress payload
        compress(comp_dict, (uint8_t *) &file_size, sizeof(file_size),
                &compressed_data, &len, offset);
        // write metadata
        write_metadata(compressed_data, FileSizeRsp, true, len - offset);
        // initialise responce
        ret = init_responce_data(EchoRsp, compressed_data, len, NULL);
    }

    return ret;
}

int ret_file_fill_write_buffer(ResponceData *rd, CompressionSegment *comp_dict,
        bool req_compr) {

    // always compress for simplicity
    OpenFileInstance *ofi = (OpenFileInstance *) rd->ptr;

    // lock during read
    pthread_mutex_lock(&ofi->lock);

    if (ofi->n_read >= ofi->n_requested) {
        pthread_mutex_unlock(&ofi->lock);
        return -1;
    }

    // compress
    uint8_t *compr_payload = NULL;
    size_t len = 0;
    size_t payload_offset = HEADER_SIZE + PAYLOAD_LEN_SIZE;
    size_t target_file_offset = 20 + payload_offset;

    // total byte offset the data begins at
    uint64_t starting_offset = ofi->n_read + ofi->offset;
    // read data
    uint64_t n_bytes = 0;
    if (rd->write_buffer_len - target_file_offset <
            ofi->n_requested - ofi->n_read) {
        n_bytes = fread(rd->write_buffer + target_file_offset, 1,
                rd->write_buffer_len - target_file_offset, ofi->file);
    } else {
        n_bytes = fread(rd->write_buffer + target_file_offset, 1,
                ofi->n_requested - ofi->n_read, ofi->file);
    }
    ofi->n_read += n_bytes;
    size_t n_read_offset = ofi->n_read;
    uint32_t session_id = ofi->session_id;

    pthread_mutex_unlock(&ofi->lock);

    // session id
    memcpy(rd->write_buffer + payload_offset, &session_id, 4);

    // write 8 byte starting offset
    uint64_t starting_offset_be = be64toh(starting_offset);
    memcpy(rd->write_buffer + payload_offset + 4, &starting_offset_be, 8);

    // remaming data length
    uint64_t n_bytes_be = be64toh(n_bytes);
    memcpy(rd->write_buffer + payload_offset + 12, &n_bytes_be, 8);

    if (req_compr) {
        // compress payload
        compress(comp_dict, rd->write_buffer + payload_offset, n_read_offset +
            target_file_offset - payload_offset, &compr_payload, &len,
            payload_offset);
        // write metadata
        write_metadata(compr_payload, RetFileRsp, true, len - payload_offset);
        // swap old write buff for new one
        free(rd->write_buffer);
        rd->write_buffer_len = len;
        rd->write_n = len;
        rd->write_buffer = compr_payload;
    } else {
        write_metadata(rd->write_buffer, RetFileRsp, false,
                n_bytes + target_file_offset - payload_offset);
        rd->write_n = target_file_offset + n_bytes;
    }
    rd->n_written = 0;
    return 0;
}

ResponceData *ret_file(bool compressed, bool req_compression, uint8_t *payload,
        uint64_t payload_len, char *dir, CompressionSegment *comp_dict,
        DecompressionTreeNode *decom_tree, OpenFileInstances *ofis) {

    uint8_t *decompressed_payload = payload;
    uint64_t decompressed_payload_n = payload_len;
    if (compressed) {
        decompress(decom_tree, payload, payload_len, &decompressed_payload,
                &decompressed_payload_n);
    }

    uint32_t session_id = 0;
    memcpy(&session_id, decompressed_payload, sizeof(session_id));

    uint64_t offset = 0;
    memcpy(&offset, decompressed_payload + sizeof(session_id), sizeof(offset));
    offset = be64toh(offset);

    uint64_t ret_size = 0;
    memcpy(&ret_size, decompressed_payload + sizeof(session_id) + sizeof(offset),
            sizeof(ret_size));
    ret_size = be64toh(ret_size);

    size_t f_name_len =decompressed_payload_n - sizeof(session_id) -
            sizeof(offset) - sizeof(ret_size);
    size_t file_path_size = strlen(dir) + f_name_len + 2;
    char *file_path = safe_malloc(file_path_size * sizeof(char));
    sprintf(file_path, "%s/%s", dir, decompressed_payload +
            sizeof(session_id) + sizeof(offset) + sizeof(ret_size));

    if (compressed) {
        free(decompressed_payload);
    }

    // check for invalid offset
    struct stat st;
    stat(file_path, &st);
    if (st.st_size < (offset + ret_size)) {
        free(file_path);
        return error();
    }

    OpenFileInstance *ofi = NULL;
    pthread_mutex_lock(&ofis->lock);
    int status = open_file(&ofi, ofis, file_path, session_id, offset, ret_size);
    pthread_mutex_unlock(&ofis->lock);
    free(file_path);
    if (status < 0) {
        return error();
    }

    uint8_t *write_buffer = safe_malloc(INIT_RET_FILE_BUFF_SIZE * sizeof(char));
    ResponceData *rd = init_responce_data(RetFileRsp, write_buffer,
            INIT_RET_FILE_BUFF_SIZE, ofi);
    // fill up the buffer
    ret_file_fill_write_buffer(rd, comp_dict, req_compression);

    return rd;
}

static int compress(CompressionSegment *comp_dict, uint8_t *uncomp_payload,
        uint64_t payload_size, uint8_t **dest, uint64_t *dest_size,
        size_t write_offset) {

    if (!comp_dict || !uncomp_payload || !dest || !dest_size) {
        return -1;
    }

    // length of uncompressed
    BitVector *bv = init_bit_vector(payload_size);

    // add each compressed bit
    uint32_t compressed_bits = 0;
    uint8_t n_bits = 0;
    uint8_t compressed_bit = 0;
    for (size_t i = 0; i < payload_size; ++i) {
        // compressed representation
        compressed_bits = comp_dict[uncomp_payload[i]].compressed;
        // number of bits. compressed is left padded
        n_bits = comp_dict[uncomp_payload[i]].compressed_len;
        // add each bit individually
        for (uint8_t i = 0; i < n_bits; ++i) {
            // shift to lsb
            compressed_bit = compressed_bits >> (n_bits - 1 - i) & 0x1;
            // push to end of bv
            bit_vector_push(bv, compressed_bit);
        }
    }

    // allocate new array
    uint8_t n_padding_bits = (8 - (bv->n_bits % 8)) % 8;
    // add 1 for n padding bits at end
    *dest_size = bv->n_bits / 8 + 1 + write_offset;
    if (n_padding_bits) {
        (*dest_size)++;
    }
    *dest = safe_malloc(*dest_size);
    // copy bit vector with offset
    memcpy(*dest + write_offset, bv->vector, *dest_size - write_offset - 1);
    // set num of padding bits at the end
    (*dest)[*dest_size - 1] = n_padding_bits;

    destroy_bit_vector(bv);
    return 0;
}

static int decompress(DecompressionTreeNode *decom_tree, uint8_t *compr_payload,
        size_t compr_payload_n, uint8_t **decompressed_payload,
        uint64_t *decompressed_payload_n) {

    if (!decom_tree || !compr_payload || !decompressed_payload ||
        !decompressed_payload_n) {
        return -1;
    }

    // return data
    *decompressed_payload = safe_malloc(INIT_DECOMPRESSED_PAYLOAD_LEN);
    uint64_t decompressed_payload_len = INIT_DECOMPRESSED_PAYLOAD_LEN;
    *decompressed_payload_n = 0;

    // total compressed bits
    uint8_t padding_len = compr_payload[compr_payload_n - 1];
    size_t compr_bit_n = (compr_payload_n - 1) * 8 - padding_len;

    // begin at root
    DecompressionTreeNode *cur_node = decom_tree;
    size_t byte_index = 0;
    size_t bit_index = 0;
    uint8_t cur_bit = 0;
    for (size_t i = 0; i < compr_bit_n; ++i) {

        // location in payload
        byte_index = i / 8;
        bit_index = i % 8;

        // extract bit
        cur_bit = (compr_payload[byte_index] >> (7 - bit_index)) & 0x1;

        // move through tree
        if (!cur_bit) {
            cur_node = cur_node->left;
        } else {
            cur_node = cur_node->right;
        }

        // leaf if no children
        if (!cur_node->left && !cur_node->right) {
            // resize if full
            if (*decompressed_payload_n >= decompressed_payload_len) {
                *decompressed_payload = safe_realloc(*decompressed_payload,
                        decompressed_payload_len * ARRAY_GROWTH_RATE);
                decompressed_payload_len *= ARRAY_GROWTH_RATE;
            }
            // insert uncompressed
            (*decompressed_payload)[*decompressed_payload_n] = cur_node->data;
            (*decompressed_payload_n)++;
            // reset to root
            cur_node = decom_tree;
        }

    }
    return 0;
}
