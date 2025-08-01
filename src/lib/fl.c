#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <fl.h>
#include <page.h>
#include <print.h>

slot* slot_list = NULL;
size_t slot_list_size = 0;

int slot_count = 0;
int unused_slots = 0;

/* 
    Since we'll be calling malloc from inside of static functions for example to allocate more 
    slots. We need a flag to mark if the new allocated chunk is for internal use or not!
*/
bool is_internal = false;

static size_t get_internal_size(bool optimize, size_t user_size);
static slot* get_slot_prev_to_internal_address(void* addr);
static slot* get_slot_for_internal_address(void* addr);
static slot* get_slot_for_user_address(void* addr);

static void fl_init();
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
    slot* prev_s = NULL;
    slot* nxt_s = NULL;
    slot* s;

    if (addr == NULL)
    {
        // no error
        return;
    }

    /* Allow access to slot list */
    if (!is_internal)
    {
        page_allow_access(slot_list, slot_list_size);
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

    /* Revoke access again to protect reads and write on slot list */
    if (!is_internal)
    {
        page_deny_access(slot_list, slot_list_size);
    }    
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

    /* The second slot points to the rest of the memory pool */
    if (size > slot_list_size)
    {
        slot_list[1].internal_address = slot_list[1].user_address = get_address(slot_list[0].internal_address, slot_list[0].internal_size);
        slot_list[1].internal_size = slot_list[1].user_size = size - slot_list[0].internal_size;
        slot_list[1].mode = FREE_SLOT;
        unused_slots--;
    }
    /* disable protection of free memory space, so that user can't read free space */
    // page_deny_access(slot_list[1].internal_address, slot_list[1].internal_size);
    /* disable protection of slot list, only allow access when its being retrieved */
    page_deny_access(slot_list, size);
}

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
    size_t page_size = PAGE_SIZE;
    size_t size = MEMORY_CREATION_SIZE; // in bytes
    size_t internal_size = 0;
    slot* s = NULL;
    size_t slack = 0;
    int count = 0;
    slot* empty_slot = NULL;
    slot* free_fit_slot = NULL;
    void* user_address = NULL;
    /* Initialize malloc data structures */
    if (slot_list == NULL)
    {
        fl_init();
    }

    /* Get the internal size */
    internal_size = get_internal_size(false, user_size);

    /* Allow access to slot list */
    if (!is_internal)
    {
        page_allow_access(slot_list, slot_list_size);
    }

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
        if (!is_internal)
        {
            page_deny_access(slot_list[0].internal_address, slot_list[0].internal_size);
        }
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
        unused_slots--;
    }
    
    /* Finally set the appropriate user address and size */
    if (is_internal)
    {
        user_address = free_fit_slot->internal_address;
        /* Set up the live page */
        page_allow_access(user_address, internal_size);
        free_fit_slot->mode = INTERNAL_USE_SLOT;
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

    /* Revoke access again to protect reads and write on slot list */
    if (!is_internal)
    {
        page_deny_access(slot_list, slot_list_size);
    }

    return user_address;
}

static size_t
get_internal_size(bool optimize, size_t user_size)
{
    size_t internal_size = 0;
    size_t slack;
    size_t page_size = PAGE_SIZE;
    int alignment = CHUNK_ALIGNMENT;

    /* because user size will always be page-aligned */
    if (is_internal) return user_size;

    /* align user size with the defined alignment of malloc */
    if ((slack = user_size % alignment) != 0)
    {
        user_size += alignment - slack;
    }

    if (optimize)
    {
        // TODO: canary bytes
        return 0;
    }
    else
    {
        /* Add a guard page in front of user page */
        internal_size = user_size + page_size;
        if ((slack = internal_size % page_size) != 0)
        {
            internal_size += page_size - slack;
        }
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
