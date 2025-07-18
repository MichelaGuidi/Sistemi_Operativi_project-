#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stddef.h> //per size_t

//dichiarazione delle funzioni pubbliche
void* my_malloc(size_t size);
void my_free(void* ptr);
void print_large_alloc_list();
void BuddyAllocator_print_bitmap();
void BuddyAllocator_print_pool();
void dump_pool(size_t num_bytes);

#endif //MY_MALLOC_H

