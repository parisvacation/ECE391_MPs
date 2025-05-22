// heap.h - Memory manager for small allocations
// 

#ifndef _HEAP_H_
#define _HEAP_H_

#include <stddef.h>

// Initializes the heap memory manager (for small objects). Called from
// memory_init(); do not call directly.

extern void heap_init(void * start, void * end);
extern char heap_initialized;

extern void * kmalloc(size_t size);
extern void * kcalloc(size_t n, size_t size);
extern void * krealloc(void * ptr, size_t size);
extern void kfree(void * ptr);

#endif // _HEAP_H_