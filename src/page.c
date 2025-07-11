
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

#include "page.h"

void* start_address = NULL;

void*
page_create(size_t size)
{
    void* s = NULL;

    /* 
        mmap chooses a page-aligned address (for most operating systems)
        This is similar to extending the heap boundary
    */
    s = mmap(start_address, size, PROT_READ | PROT_WRITE, 
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (s == NULL)
    {
        // error
        return s;
    }

    /* type-cast to (char*) to perform pointer arithmatic; hint: "void* has no type" */
    start_address = (void*)((char*)s + size);
    return s;
}

void
page_allow_access(void* address, size_t size)
{
    if (!address) return;
    /* Check if the address is page-aligned */
    if ((uintptr_t)address % PAGE_SIZE)
    {
        // error
        return;
    }

    if (mprotect(address, size, PROT_READ | PROT_WRITE) == -1)
    {
        //error
        return;
    }
}

void
page_deny_access(void* address, size_t size)
{
    if (!address) return;
    /* Check if the address is page-aligned */
    if ((uintptr_t)address % PAGE_SIZE)
    {
        // error
        return;
    }

    if (mprotect(address, size, PROT_NONE) == -1)
    {
        //error
        return;
    }
}
