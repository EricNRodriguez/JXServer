#ifndef COMP2017_ASSIGNMENT_3_MEMORY_H
#define COMP2017_ASSIGNMENT_3_MEMORY_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define ARRAY_GROWTH_RATE 2

/** @brief Wraps malloc, calls perror and exits on error.
 *
 *  If malloc returns null, perror is called and program exits with status
 *  EXIT_FAILURE.
 *
 *  @param size : number of bytes to be allocated.
 *  @return address of allocated memory.
 */
void *safe_malloc(size_t size);

/** @brief Wraps realloc, calls perror and exits on error.
 *
 *  If realloc returns null, perror is called and program exits with status
 *  EXIT_FAILURE.
 *
 *  @param old : address of memory to be reallocated.
 *  @param size : number of bytes to be allocated.
 *  @return address of reallocated memory.
 */
void *safe_realloc(void *old, size_t size);

/** @brief Wraps calloc, calls perror and exits on error.
 *
 *  If calloc returns null, perror is called and program exits with status
 *  EXIT_FAILURE.
 *
 *  @param size : size of members to be allocated.
 *  @param nmeb : number of members to be allocated.
 *  @return address of allocated memory.
 */
void *safe_calloc(size_t nmeb, size_t size);

#endif //COMP2017_ASSIGNMENT_3_MEMORY_H
