#include "compression_dict.h"

/** @brief Retrieves variable number of bits from byte array.
 *
 *  starting_bit_index indicates the bit offset, with LSB as left most bit.
 *  Variable number of 0 bits are padded on the left to align with a byte
 *  boundary.
 *
 *  @param starting_bit_index : bit index (from left).
 *  @param n_bits : Number of bits requested.
 *  @param bytes : byte array.
 *  @param bytes_len : Length of byte array.
 *  @return Requested bits, left padded to byte boundary with 0 bits.
 */
static uint64_t get_bits(size_t starting_bit_index, size_t n_bits,
        uint8_t *bytes, size_t bytes_len);

CompressionSegment *parse_compression_dictionary() {
    CompressionSegment *dict = safe_malloc(COMPRESSION_DICT_LEN *
            sizeof(CompressionSegment));

    if (access(COMPRESSION_DICT_FILE_NAME, F_OK)) {
        printf("unable to load %s | does not exist\n", COMPRESSION_DICT_FILE_NAME);
        exit(EXIT_FAILURE);
    }

    FILE *dict_bin = fopen(COMPRESSION_DICT_FILE_NAME, "rb");
    // get size of file
    fseek(dict_bin, 0, SEEK_END);
    long dict_f_size = ftell(dict_bin);
    fseek(dict_bin, 0, SEEK_SET);

    // allocate array
    uint8_t *raw_dict = safe_malloc(dict_f_size);
    // read into buffer
    if (fread(raw_dict, dict_f_size, 1, dict_bin) < 1) {
        printf("failed to parse compression dict\n");
        exit(EXIT_FAILURE);
    };

    size_t current_bit_index = 0;
    for (size_t i = 0; i < COMPRESSION_DICT_LEN; ++i) {
        dict[i].uncompressed = i;
        // get first byte
        dict[i].compressed_len = get_bits(current_bit_index, 8, raw_dict,
                dict_f_size);
        // increment by a byte
        current_bit_index += 8;
        // get compression bits
        dict[i].compressed = get_bits(current_bit_index, dict[i].compressed_len,
                raw_dict, dict_f_size);
        // increment index by number of bits
        current_bit_index += dict[i].compressed_len;
    }

    free(raw_dict);
    fclose(dict_bin);
    return dict;
}

static uint64_t get_bits(size_t starting_bit_index, size_t n_bits, uint8_t *bytes,
        size_t bytes_len) {
    uint64_t bits = 0;

    size_t byte_index = starting_bit_index / 8;
    size_t bit_index = starting_bit_index % 8;

    for (size_t i = 0; i < n_bits; ++i) {
        if (bit_index >= 8) {
            bit_index = 0;
            byte_index++;
        }

        // shift into appropriate position
        bits |= (((bytes[byte_index] >> (7 - bit_index)) & 0x01) <<
                (n_bits - i - 1));
        bit_index++;
    }

    return bits;
}

void destroy_compression_dict(CompressionSegment *dict) {
    free(dict);
    return;
}