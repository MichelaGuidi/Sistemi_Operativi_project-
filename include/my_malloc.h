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

//funzioni per scrittura e lettura (allocazioni grandi)
int my_write_large_alloc(void* ptr, size_t offset, const void* data, size_t data_size);
int my_read_large_alloc(void* ptr, size_t offset, void* buffer, size_t buffer_size);

//funzioni per scrittura e lettura (buddy allocator)
int my_write_buddy_alloc(void* ptr, const char* data, size_t size);
int my_read_buddy_alloc(void* ptr, char* buffer, size_t size);

#endif //MY_MALLOC_H

