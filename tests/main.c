#include "my_malloc.h"

#include <stdio.h> //per printf()
#include <stdlib.h> // per EXIT_SUCCESS, EXIT_FAILURE
#include <string.h> // per memset
#include <time.h> // per time (srand)

#define MIN_LARGE_ALLOC_SIZE 1024 //minima dimensione per mmap
#define MAX_LARGE_ALLOC_SIZE (1024*1024) //1MB, dimensione massima per test mmap

#define NUM_ALLOCS 10 //numero di allocazioni in un ciclo

int main(){
    printf("---Test iniziale del pseudo malloc---\n");
    printf("Allocazioni Grandi(mmap).\n\n");

    srand(time(NULL)); //inizializzazione generatore numeri casuali per le dimensioni

    void* ptrs[NUM_ALLOCS];
    size_t sizes[NUM_ALLOCS];
    int i;

    printf("Test 1: allocazione e deallocazione singola (es. 2KB): \n");
    void* single_ptr = my_malloc(2048); //dimensione grande
    if (single_ptr){
        printf("allocazione singola (2048 byte): %p\n", single_ptr);
        memset(single_ptr, 0xAA, 2048);
        if (((unsigned char*) single_ptr)[0] == 0xAA){
            printf (" -> scrittura/lettura ok \n");
        } else {
            fprintf(stderr, " -> errore: scrittura/lettura fallita \n");
        }
        my_free(single_ptr);
        printf(" -> deallocazione singola \n");
    } else {
        fprintf(stderr, "errore: fallita singola allocazione (2048 byte) \n");
        return EXIT_FAILURE;
    }
    printf("\n");


    printf("Test 2: allocazioni multiple con dimensioni casuali (>= %d byte):\n", MIN_LARGE_ALLOC_SIZE);
    for (int i = 0; i < NUM_ALLOCS; ++i){
        sizes[i] = (rand()%(MAX_LARGE_ALLOC_SIZE - MIN_LARGE_ALLOC_SIZE + 1)) + MIN_LARGE_ALLOC_SIZE;
        ptrs[i] = my_malloc(sizes[i]);
        if (ptrs[i] == NULL){
            fprintf(stderr, "Errore: my_malloc(%zu byte) fallito per allocazione #%d.\n", sizes[i], i);
            sizes[i] = 0;
        } else {
            printf("allocato #%d: %p, size %zu\n", i, ptrs[i], sizes[i]);
            memset(ptrs[i], (i%255) +1, sizes[i]);
        }
    }

    printf("allocazioni multiple completate.\n");
    printf("\nTest 2b: Deallocazione in ordine casuale...\n"); 
    int free_order[NUM_ALLOCS]; 
    for (i = 0; i < NUM_ALLOCS; ++i) free_order[i] = i; 
    // Shuffle dell'ordine di deallocazione 
    for (i = 0; i < NUM_ALLOCS; ++i) {
        int j = rand() % NUM_ALLOCS; 
        int temp = free_order[i]; 
        free_order[i] = free_order[j]; 
        free_order[j] = temp; 
    } 
    for (i = 0; i < NUM_ALLOCS; ++i) { 
        int idx_to_free = free_order[i]; 
        if (ptrs[idx_to_free] != NULL && sizes[idx_to_free] > 0) { 
            printf(" Deallocando #%d (%p, size %zu)...\n", idx_to_free, ptrs[idx_to_free], sizes[idx_to_free]); 
            my_free(ptrs[idx_to_free]); 
            ptrs[idx_to_free] = NULL; // Previene doppi tentativi di free 
        } 
    } 
    printf(" Deallocazioni multiple completate.\n"); 
    return EXIT_SUCCESS; 
}