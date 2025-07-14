#ifndef FL_H
#define FL_H

#include <stdio.h>
#include <stdlib.h>

#define get_address(base, offset) \
    (void*)((char*)base + offset)

/**
 * The mode corresponding to each slot, indicates the status of the memory buffer
 * 
 * - IOTA_SLOT:         The slot is uninitialized, neither free nor allocated
 * - FREE_SLOT:         The memory at this slot is free and can be given to a buffer
 * - ALLOCATED_SLOT:    The memory at this slot is occupied by some buffer i.e. already taken
 * - PROTECTED_SLOT:    The freed buffer that can't be allocated again     
 * - INTERNAL_USE_SLOT: The memory corresponding to this slot is used by this library (like the allocation list)
 */
typedef enum _mode
{
    IOTA_SLOT = 0,
    FREE_SLOT,
    ALLOCATED_SLOT,
    PROTECTED_SLOT,
    INTERNAL_USE_SLOT,
} mode;

/**
 * The slot structure contains the metadata of the buffer except the contents
 */
typedef struct _slot
{
    void* internal_address;    /**< The actual virtual address  */
    void* user_address;        /**< The user address */
    size_t internal_size;      /**< The size of the memory with metadata and original data */
    size_t user_size;          /**< The size of original data */
    mode mode;                 /**< The mode of the slot */
} slot;

/**
 * fault-line version of malloc()
 * @param size The size of buffer to be allocated
 */
void* malloc(size_t size);

/**
 * fault-line version of free()
 * @param arr The user address of buffer memory to be freed
 */
void free(void* arr);

#endif // FL_H