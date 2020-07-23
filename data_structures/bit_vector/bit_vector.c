#include "bit_vector.h"

BitVector *init_bit_vector(size_t init_size) {
    BitVector *bv = safe_malloc(sizeof(BitVector));
    bv->vector = safe_calloc(init_size, 1);
    bv->vector_len = init_size;
    bv->n_bits = 0;

    return bv;
}

void bit_vector_push(BitVector *bv, uint8_t bit) {
    if (!bv) {
        return;
    }

    size_t byte_index = bv->n_bits / 8;
    size_t bit_offset = bv->n_bits % 8;

    // expand if full
    if (byte_index >= bv->vector_len) {
        bv->vector = safe_realloc(bv->vector, bv->vector_len *
                            VECTOR_GROWTH_RATE);
        // initialise remaining to 0
        memset(bv->vector + bv->vector_len, 0, bv->vector_len);
        bv->vector_len *= VECTOR_GROWTH_RATE;
    }

    // insert
    bv->vector[byte_index] |= ((0x1 & bit) << (7 - bit_offset));
    bv->n_bits++;

    return;

}

void destroy_bit_vector(BitVector *bv) {
    if (!bv) {
        return;
    }

    free(bv->vector);
    free(bv);
    return;
}