#ifndef ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_BIT_VECTOR_H
#define ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_BIT_VECTOR_H

#include "../../memory/memory.h"

#include <stdint.h>
#include <string.h>

#define VECTOR_GROWTH_RATE 2

typedef struct {
    uint8_t *vector;
    size_t vector_len;
    size_t n_bits;
} BitVector;

/** @brief Initialises bit vector.
 *
 *  Allocates vector to to init_size.
 *
 *  @param init_size : Init bit vector length (in bytes).
 *  @return Bit vector instance.
 */
BitVector *init_bit_vector(size_t init_size);

/** @brief Adds bit to vector.
 *
 *  If bv is NULL, nothing is done. Otherwise, bit is added to next available
 *  index. Vector acts as a stack with no pop function.
 *
 * @param bv : Bit vector instance.
 * @param bit : bit to be pushed.
 */
void bit_vector_push(BitVector *bv, uint8_t bit);

/** @brief Destroys bit vector.
 *
 *  All allocated memory is released.
 *
 * @param bv : Bit vector instance.
 */
void destroy_bit_vector(BitVector *bv);

#endif //ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_BIT_VECTOR_H