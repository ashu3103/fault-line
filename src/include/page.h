#ifndef PAGE_H
#define PAGE_H

#include <unistd.h>

#define PAGE_SIZE            (size_t)sysconf(_SC_PAGESIZE)

#define CHUNK_ALIGNMENT      16           // Assume that every chunk in malloc is 16 byte aligned
#define MEMORY_CREATION_SIZE 1024 * 1024  // Create this much memory in a single request
#define CANARY_BYTE          0xFF
// #define NUM_CANARY_BYTES     2            // Total number of canary bytes

/**
 * Create a memory block of a given size
 * @param size The size of memory block
 * @return The address of the newly created memory block
 */
void* page_create(size_t size);

/**
 * Allow read/write access to memory locations from [address, address+size-1]
 * @param address The address
 * @param size The size
 */
void page_allow_access(void* address, size_t size);

/**
 * Deny all access to memory locations from [address, address+size-1]
 * @param address The address
 * @param size The size
 */
void page_deny_access(void* address, size_t size);

#endif // PAGE_H