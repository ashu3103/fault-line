# fault-line

A minimal malloc() and free() implementation in C that do more than just allocations!

## Highlights

It checks and reports some of the most common errors encountered by programmers like:

- calling free() on the same memory block more than once (double free)
- attempting to free() memory that was never allocated (uninitialized heap free)
- writing beyond the bounds of an allocated memory buffer (overruns and underruns)

The foundational architecture of this tool was adopted from a similar tool [electric fence](https://github.com/kallisti5/ElectricFence), however this tool was very poor in performance and wastes a lot of memory for small allocations, since it reserves a whole page as guard page for each allocation. fault-line understands this and adapts a hybrid approach, a mix of guard pages and canary-bytes to detect these errors while making allocations efficient.

## Build

The only real build requirement is a C compiler and the Cmake (version >= 3.14) build system.

```
mkdir build
cd build
cmake .. && make
```

The above commands will generate the static (`.a`) and the dynamic (`.so`) libraries.

## Usage

Using fault-line is easy. You can use it multiple ways:

- Link the generated static `libfl.a` archive into your application at build
- Preload the generated shared library `libfl.so` at runtime via `LD_PRELOAD=./path/to/library/libfl.so  /bin/myapplication`
