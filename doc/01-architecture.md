## Architecture

fault-line adds an extra layer of virtualization on top of the heap memory given by the operating system. Like many virtual machine (say f.ex. JIT), it grabs a large block of memory from the OS and takes full control over how that memory is managed and allocated to the user.

(an image displaying virtualization)

### Slot based approach

Unlike the traditional free-list approach (which typically uses a linked list), fault-line uses a registry of slots to manage memory. Usually, an entire memory page is set aside to store this slot list.

Each slot holds metadata about a memory chunk it may point to (though a slot can also be empty). The information stored in a slot includes:

- internal address: the actual virtual address of the memory chunk
- internal size: the full size of that memory chunk
- user address: the part of the memory chunk that is exposed to the user
- user size: how much of the memory the user is allowed to access
- mode: indicates what kind of slot it is

### Slot modes

From the user's point of view, each slot can be in one of the following states:

- UNUSED_SLOT: The slot is empty and not associated with any memory.
- FREE_SLOT: The slot points to a memory chunk that is currently available for allocation.
- ALLOCATED_SLOT: The slot refers to a memory chunk that has been allocated and is in use by the end user.


(image displaying slots and memory chunk in action)
