#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include <fl.h>
#include <page.h>
#include <print.h>

/* States of slot list */
slot* slot_list = NULL;
size_t slot_list_size = 0;

int slot_count = 0;
int unused_slots = 0;

/* States of bin allocator */
int number_of_bins = 0;
size_t threshold = 0; // should be compared with internal size

/* 
    Since we'll be calling malloc from inside of static functions for example to allocate more 
    slots. We need a flag to mark if the new allocated chunk is for internal use or not!
*/
bool is_internal = false;
bool is_bin_internal = false;

static size_t get_internal_size(bool* use_bin_alloc, size_t user_size);
static slot* get_slot_prev_to_internal_address(void* addr);
static slot* get_slot_for_internal_address(void* addr);
static slot* get_slot_for_user_address(void* addr);
static bool check_canary_bytes(void* addr, uint8_t canary_byte);

/* wrappers */
static void allow_access_internal();
static void deny_access_internal();
static void* pages_alloc(size_t user_size, size_t internal_size);
static void* bin_page_alloc(size_t user_size, size_t internal_size);

static void fl_init();
static void fl_bin_allocator_init();
static void* fl_memalign(size_t user_size);
static void fl_allocate_more_slots();

void* malloc(size_t size)
{
    void* allocation = NULL;
    if (slot_list == NULL)
    {
        fl_init();
    }

    allocation = fl_memalign(size);
    return allocation;
}

void free(void* addr)
{
    size_t page_size = PAGE_SIZE;
    slot* prev_s = NULL;
    slot* nxt_s = NULL;
    slot* s;

    if (addr == NULL)
    {
        // no error
        return;
    }

    /* Allow access to slot list */
    allow_access_internal();

    /* Check if the address is not page-aligned, it should be in bin allocated area */
    if ((uintptr_t)addr % page_size)
    {
        if ((uintptr_t)addr % CHUNK_ALIGNMENT)
        {
            fl_error("free(): free of unintialized heap\n");
        }

        /* get the metadata and check if the chunk is already free */
        uintptr_t* metadata_ptr = (uintptr_t*)(addr - 2 * CHUNK_ALIGNMENT);
        uintptr_t metadata = *metadata_ptr;
        if (!get_bin_alloc_status(metadata))
        {
            fl_error("free(): double free of address: %a\n", addr);
        }

        /* check canary bytes */
        uint8_t ind = *((uint8_t*)addr - CHUNK_ALIGNMENT);
        if (!check_canary_bytes(get_address(addr, -1*CHUNK_ALIGNMENT), ind))
        {
            fl_error("free(): segmentation fault\n");
        }

        /* unset the metadata */
        *metadata_ptr = get_bin_alloc_next(metadata);

        /* find the extra page */
        uintptr_t* bin_address = (uintptr_t*)get_address(slot_list[1].internal_address, ind*CHUNK_ALIGNMENT);
        uintptr_t* link = (uintptr_t*)(*bin_address);
        uintptr_t* prev_link = NULL;
        bool prev_is_blank = true;

        while (link)
        {
            /* at next page */
            if ((uintptr_t)link % page_size == 0)
            {
                if (prev_is_blank && prev_link)
                {
                    /* set previous link to this link */
                    *prev_link = (uintptr_t)link;
                    /* find the allocated bin slot using link address */
                    s = get_slot_for_internal_address((void*)link);
                    if (s->mode != ALLOCATED_BIN_SLOT)
                    {
                        fl_error("free(): internal error\n");
                    }

                    goto coalesce;
                }
                prev_link = link;
                prev_is_blank = true;
            }

            if (get_bin_alloc_status(*link))
            {
                prev_is_blank = false;
            }
            link = (uintptr_t*)get_bin_alloc_next(*link);
        }

        goto finish;
    }

    /* get the slot which is associated with the user address */
    s = get_slot_for_user_address(addr);

    if (s == NULL)
    {
        fl_error("free(): free of unintialized heap\n");
    }

    if (s->mode == INTERNAL_USE_SLOT && !is_internal)
    {
        fl_error("free(): how did u get this address??\n");
    }

    if (s->mode == FREE_SLOT)
    {
        fl_error("free(): double free of address: %a\n", s->user_address);
    }

coalesce:
    /* try to coalesce with the neighbouring slots */
    prev_s = get_slot_prev_to_internal_address(s->internal_address);
    nxt_s = get_slot_for_internal_address(get_address(s->internal_address, s->internal_size));

    /* coalesce previous slot */
    if (prev_s != NULL && prev_s->mode == FREE_SLOT)
    {
        prev_s->internal_size = prev_s->internal_size + s->internal_size;
        prev_s->mode = FREE_SLOT;
        /* mark previous slot as unused */
        s->internal_address = s->user_address = 0;
        s->internal_size = s->user_size = 0;
        s->mode = IOTA_SLOT;

        s = prev_s;
        unused_slots++;
    }

    /* coalesce next slot */
    if (nxt_s != NULL && nxt_s->mode == FREE_SLOT)
    {
        s->internal_size = nxt_s->internal_size + s->internal_size;
        /* mark next slot as unused */
        nxt_s->internal_address = nxt_s->user_address = 0;
        nxt_s->internal_size = nxt_s->user_size = 0;
        nxt_s->mode = IOTA_SLOT;
        unused_slots++;
    }

    s->user_address = s->internal_address;
    s->user_size = s->internal_size;
    s->mode = FREE_SLOT;

    page_deny_access(s->internal_address, s->internal_size);
finish:
    /* Revoke access again to protect reads and write on slot list and bin allocator */
    deny_access_internal();
}

static void
fl_init()
{
    size_t page_size = PAGE_SIZE; // in bytes
    size_t size = MEMORY_CREATION_SIZE; // in bytes
    size_t slack;
  
    slot_list_size = page_size;

    if (slot_list_size > size)
    {
        size = slot_list_size;
    }
    /* make size a multiple of page size */
    if ((slack = size % page_size) != 0)
    {
        size += page_size - slack;
    }

    /* Ask for a decent amount of memory from the operating system */
    slot_list = page_create(size);
    /* Reserve first page for the slots and rest as memory pool */
    slot_count = page_size / sizeof(slot);
    /* initialize the slot area */
    memset(slot_list, 0, slot_list_size);

    unused_slots = slot_count;
    /* The first slot should always points to the slot list itself */
    slot_list[0].internal_address = slot_list[0].user_address = (void*)slot_list;
    slot_list[0].internal_size = slot_list[0].user_size = slot_list_size;
    slot_list[0].mode = INTERNAL_USE_SLOT;
    unused_slots--;

    /* The second slot points to the bin allocator */
    if (size > slot_list_size)
    {
        slot_list[1].internal_address = slot_list[1].user_address = get_address(slot_list[0].internal_address, slot_list[0].internal_size);
        slot_list[1].internal_size = slot_list[1].user_size = page_size; // dedicate a page for bin allocator
        slot_list[1].mode = INTERNAL_USE_SLOT;
        fl_bin_allocator_init();
        unused_slots--;
    }

    /* The third slot points to the rest of the memory pool */
    if (size > slot_list_size + page_size)
    {
        slot_list[2].internal_address = slot_list[2].user_address = get_address(slot_list[1].internal_address, slot_list[1].internal_size);
        slot_list[2].internal_size = slot_list[2].user_size = size - (slot_list[0].internal_size + slot_list[1].internal_size);
        slot_list[2].mode = FREE_SLOT;
        unused_slots--;
    }

    print("slot list address: %a\n", slot_list[0].internal_address);
    print("bin allocator address: %a\n", slot_list[1].internal_address);
    print("central free list address: %a\n", slot_list[2].internal_address);
    /* disable protection of slot list, only allow access when its being retrieved */
    page_deny_access(slot_list, size);
}

static void
fl_bin_allocator_init()
{
    size_t page_size = PAGE_SIZE; // in bytes
    size_t chunk_alignment = CHUNK_ALIGNMENT;
    /* bin size must be less than page size and a multiple of chunk alignment */

    number_of_bins = page_size / chunk_alignment;
    while (number_of_bins && get_bin_size(number_of_bins-1) > page_size)
    {
        /* make sure the maximum bin size is less than page size */
        number_of_bins--;
    }
    memset(slot_list[1].internal_address, 0, slot_list[1].internal_size);

    /* threshold is the maximum size of the bin */
    threshold = get_bin_size(number_of_bins - 1);
    print("threshold: %U\n", threshold);
}

// TODO: testing pending
static void
fl_allocate_more_slots()
{
    size_t new_size;
    slot* new_slot_list = NULL;
    slot* old_slot_list = slot_list;
    int new_slot_count = 0;
    size_t page_size = PAGE_SIZE;

    is_internal = true;
    new_size = slot_list_size + page_size;
    
    /* Find a free space of that can accomodate current size and one extra page */
    new_slot_list = malloc(new_size);
    
    /* Copy the previous slot list to new slot list */
    memset(new_slot_list, 0, new_size);
    memcpy(new_slot_list, old_slot_list, slot_list_size);
    
    /* Update the global states */
    slot_list = new_slot_list;
    slot_list_size = new_size;
    new_slot_count = new_size / sizeof(slot);
    unused_slots = new_slot_count - (slot_count - unused_slots);
    slot_count = new_slot_count;

    /* mark the old allocation as free */
    free(old_slot_list);
    
    is_internal = false;
}

static void*
fl_memalign(size_t user_size)
{
    size_t internal_size = 0;
    bool use_bin_alloc = false;
    /* Initialize malloc data structures */
    if (slot_list == NULL)
    {
        fl_init();
    }

    /* Get the internal size */
    internal_size = get_internal_size(&use_bin_alloc, user_size);
    print("internal size: %U\n", internal_size);
    if (!use_bin_alloc)
    {
        return pages_alloc(user_size, internal_size);
    }
    else
    {
        print("bin alloc\n");
        return bin_page_alloc(user_size, internal_size);
    }
}

static void*
pages_alloc(size_t user_size, size_t internal_size)
{
    size_t page_size = PAGE_SIZE;
    size_t size = MEMORY_CREATION_SIZE; // in bytes
    slot* s = NULL;
    size_t slack = 0;
    int count = 0;
    slot* empty_slot = NULL;
    slot* free_fit_slot = NULL;
    void* user_address = NULL;

    /* Allow access to internal data structures */
    allow_access_internal();

    /* Check if slots are exhausted, atleast 8 unused slots must be present */
    if (!is_internal && unused_slots <= 8)
    {
        fl_allocate_more_slots();
    }

    /**
     * Find the free space using best-fit algorithm
     * 
     * Only a free slot can become an allocated slot or internal use slot
     * 
     * Case 1: One or many free slots
     * - try to choose the best fit
     * - if found and slot size > internal size, divide it into two free slots and allocate one
     * Case 2: No free slot or no fit free slot
     * - allocate a new chunk by requesting from OS
     * 
     * In both the cases we will be needing unused slot, for case 1 we will divide the free slots into
     * two and use an unused slot to mark it free (while first free slot will be marked allocated), while in
     * case 2, we will create a new free memory chunk.
     * 
     */
    for (s = slot_list, count = 0; count < slot_count; count++)
    {
        if (s->mode == FREE_SLOT && s->internal_size >= internal_size) // find best fit slot
        {
            if (!free_fit_slot || s->internal_size < free_fit_slot->internal_size)
            {
                free_fit_slot = s;
                /* just in case we get an exact size */
                if (free_fit_slot->internal_size == internal_size && empty_slot != NULL)
                {
                    break;
                }
            }
        }
        else if (!empty_slot && s->mode == IOTA_SLOT) // get one unused slot (it is guranteed that we get an unused slot)
        {
            empty_slot = s;
        }
        s++;
    }
    /* if no free slot found, allocate a new chunk */
    if (!free_fit_slot)
    {
        if (!empty_slot)
        {
            fl_error("malloc(): no empty slots found\n"); // TODO: just exit no print
        }

        if (internal_size > size)
        {
            size = internal_size;
        }
        
        if ((slack = size % page_size) != 0)
        {
            size += page_size - slack;
        }
        
        empty_slot->internal_address = empty_slot->user_address = page_create(size);
        empty_slot->internal_size = empty_slot->user_size = size;
        empty_slot->mode = FREE_SLOT;
        unused_slots--;
        // TODO: try to coalesce with the previous chunk, when impl free()
        
        /* Deny access to newly created free memory */
        page_deny_access(empty_slot->internal_address, empty_slot->internal_size);

        /* Restore all states that was before memalign */
        deny_access_internal();
        /* new free space created, try again */
        return fl_memalign(user_size);
    }

    /* Divide the free space into two */
    if (free_fit_slot->internal_size > internal_size)
    {
        empty_slot->internal_address = empty_slot->user_address = get_address(free_fit_slot->internal_address, internal_size);
        empty_slot->internal_size = empty_slot->user_size = free_fit_slot->internal_size - internal_size;
        free_fit_slot->internal_size = internal_size;
        empty_slot->mode = FREE_SLOT;
        print("empty slot: %a\n", empty_slot);
        print("empty slot address: %a\n", empty_slot->internal_address);
        print("empty slot size: %U\n", empty_slot->internal_size);
        print("empty slot mode: %d\n", empty_slot->mode);
        unused_slots--;
    }

    /* Finally set the appropriate user address and size */
    if (is_internal)
    {
        user_address = free_fit_slot->internal_address;
        /* Set up the live page */
        page_allow_access(user_address, internal_size);
        free_fit_slot->mode = (!is_bin_internal) ? INTERNAL_USE_SLOT : ALLOCATED_BIN_SLOT;
        print("free slot slot: %a\n", free_fit_slot);
        print("free slot address: %a\n", free_fit_slot->internal_address);
        print("free slot size: %U\n", free_fit_slot->internal_size);
        print("free slot mode: %d\n", free_fit_slot->mode);
    }
    else
    {
        user_address = get_address(free_fit_slot->internal_address, page_size); // reserve one page in free page for dead page
        if (internal_size - page_size > user_size)
        {
            /* Set up the live page */
            page_allow_access(user_address, internal_size - page_size);
        }
        free_fit_slot->mode = ALLOCATED_SLOT;
    }
    free_fit_slot->user_address = user_address;
    free_fit_slot->user_size = user_size;

    /* Revoke access again to protect reads and write on slot list and bin allocator */
    deny_access_internal();

    return user_address;
}

static void*
bin_page_alloc(size_t user_size, size_t internal_size)
{
    uint8_t ind = -1;
    void* user_address = NULL;
    uintptr_t* bin_list_addr = NULL;
    uintptr_t* link = NULL;
    uintptr_t metadata = 0;
    int chunks = 0;
    size_t page_size = PAGE_SIZE;
    
    allow_access_internal();
    /* get bin allocator index using internal_size */
    ind = get_bin_index(internal_size);
    /* if not available, request a size of a page and divide it into (index+3)*16 chunks */
    bin_list_addr = (uintptr_t*)get_address(slot_list[1].internal_address, ind * CHUNK_ALIGNMENT);
    link = bin_list_addr;

    if (!(*bin_list_addr))
    {
demand:
        // request a page with internal privilege
        is_internal = true;
        is_bin_internal = true;

        void* start = malloc(page_size);
        memset(start, 0, page_size);
        print("start address: %a\n", start);
        /* Divide the new chunk into bins */
        chunks = page_size / internal_size;
        uintptr_t bound = (uintptr_t)get_address(start, page_size);
        for (int i = 0; i < chunks; i++)
        {
            void* bin_cur = get_address(start, i*internal_size);
            void* bin_nxt = get_address(start, (i+1)*internal_size);
            print("bin_%d curr address: %a\n", i, bin_cur);
            print("bin_%d next address: %a\n", i, bin_nxt);
            print("bin_%d next uint: %U\n", i, (uintptr_t)bin_nxt);
            /* set the address of next bin in metadata */
            if ((uintptr_t)bin_nxt < bound)
            {
                *((uintptr_t*)bin_cur) = (uintptr_t)bin_nxt;
            }
            print("bin_%d next value: %U\n", i, (uintptr_t)(*((uintptr_t*)bin_cur)));
            /* set the canary bytes */
            memset(get_address(bin_cur, CHUNK_ALIGNMENT), ind, CHUNK_ALIGNMENT);
        }
        
        // restore the state of the chunk by just modifying the address
        *link = (uintptr_t)start | (*link & 1);
        // revoke internal privilege
        is_internal = false;
        is_bin_internal = false;
    }

    /* find a free bin slot */
    uintptr_t* ptr = (uintptr_t*)(*bin_list_addr);
    metadata = *ptr;

    if (metadata == 0)
    {
        fl_error("bin allocator not found");
    }
    
    do {
        link = ptr;
        // check if the bin is free
        if (!get_bin_alloc_status(metadata))
        {
            user_address = get_address((void*)ptr, 2*CHUNK_ALIGNMENT);
            *ptr = metadata | 1UL;
            goto finish;
        }

        ptr = (uintptr_t*)get_bin_alloc_next(metadata);
        metadata = *ptr;
    }
    while(metadata);
    print("link: %a\n", (void*)link);
    goto demand;

finish:
    print("metadata of user address: %U\n", (uintptr_t)(*((uintptr_t*)get_address(user_address, -2*CHUNK_ALIGNMENT))));
    deny_access_internal();
    return user_address;
}

static size_t
get_internal_size(bool* use_bin_alloc, size_t user_size)
{
    size_t internal_size = 0;
    size_t slack;
    size_t page_size = PAGE_SIZE;
    int alignment = CHUNK_ALIGNMENT;

    /* because user size will always be page-size multiple */
    if (is_internal) {

        if (user_size % page_size != 0)
        {
            fl_error("user size must be a multiple of page size for internal memory demands\n");
        }
        return user_size;
    }

    /* align user size with the defined alignment of malloc */
    if ((slack = user_size % alignment) != 0)
    {
        user_size += alignment - slack;
    }

    if (user_size <= (threshold - 2 * CHUNK_ALIGNMENT))
    {
        /* Add space for metadata and canary bytes before user space */
        internal_size = user_size + 2 * CHUNK_ALIGNMENT;
        if ((slack = internal_size % CHUNK_ALIGNMENT) != 0)
        {
            internal_size += CHUNK_ALIGNMENT - slack;
        }
        *use_bin_alloc = true;
        return internal_size;
    }

    /* Add space for guard page in front of user space */
    internal_size = user_size + page_size;
    if ((slack = internal_size % page_size) != 0)
    {
        internal_size += page_size - slack;
    }

    return internal_size;
}

static slot*
get_slot_for_user_address(void* addr)
{
    slot* s = NULL;
    int count = 0;

    /* get the slot */
    for (s = slot_list, count = slot_count; count; count--)
    {
        if (s->user_address == addr && !(s->mode == IOTA_SLOT))
        {
            return s;
        }
        s++;
    }

    return NULL;
}

static slot*
get_slot_prev_to_internal_address(void* addr)
{
    slot* s = slot_list;
    int count = 0;

    for (; count < slot_count; count++)
    {
        if (addr == get_address(s->internal_address, s->internal_size))
        {
            return s;
        }
        s++;
    }

    return NULL;
}

static slot*
get_slot_for_internal_address(void* addr)
{
    slot* s = slot_list;
    int count = 0;

    for (; count < slot_count; count++)
    { 
        if (s->internal_address == addr)
        {
            return s;
        }
        s++;
    }

    return NULL;
}

static void
allow_access_internal()
{
    /* if called for internal data structure, we can be sure that access is allowed */
    if (is_internal) return;
    /* allow access to slot list */
    page_allow_access(slot_list, slot_list_size);
    /* allow access to bin allocator */
    page_allow_access(slot_list[1].internal_address, slot_list[1].internal_size);
}

static void
deny_access_internal()
{
    /* if called for internal data structure, we can be sure that access is allowed */
    if (is_internal) return;
    /* deny access to bin allocator */
    page_deny_access(slot_list[1].internal_address, slot_list[1].internal_size);
    /* allow access to slot list */
    page_deny_access(slot_list, slot_list_size);
}

static bool
check_canary_bytes(void* addr, uint8_t canary_byte)
{
    size_t chunk_alignment = CHUNK_ALIGNMENT;

    for (size_t i = 0; i< chunk_alignment; i++)
    {
        if (*((uint8_t*)get_address(addr, i)) != canary_byte)
        {
            return false;
        }
    }
    return true;
}
