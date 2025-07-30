
#include <stdint.h>
#include <sys/mman.h>

#include <page.h>
#include <print.h>

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
        fl_error("page_create: unable to create a memory block with mmap\n");
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
        fl_error("page_allow_access: address: %a is not page aligned\n", address);
    }

    if (mprotect(address, size, PROT_READ | PROT_WRITE) == -1)
    {
        fl_error("page_allow_access: mprotect error\n");
    }
}

void
page_deny_access(void* address, size_t size)
{
    if (!address) return;
    /* Check if the address is page-aligned */
    if ((uintptr_t)address % PAGE_SIZE)
    {
        fl_error("page_deny_access: address: %a is not page aligned\n", address);
    }

    if (mprotect(address, size, PROT_NONE) == -1)
    {
        fl_error("page_deny_access: mprotect error\n");
    }
}
