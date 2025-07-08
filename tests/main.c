#include "my_malloc.h"

#include <stdio.h> //per printf()
#include <stdlib.h> // per EXIT_SUCCESS, EXIT_FAILURE
#include <string.h> // per memset
#include <time.h> // per time (srand)

#define NUM_RANDOM_ALLOCS 2000 //numero di allocazioni e deallocazioni casuali
#define MAX_RANDOM_SIZE (16*1024) // dimensione massima delle richieste di memoria per allocazioni casuali (16KB)

#ifndef PAGE_SIZE_FOR_TESTS
#define PAGE_SIZE_FOR_TESTS 4096
#endif
#define MALLOC_THRESHOLD_FOR_TESTS (PAGE_SIZE_FOR_TESTS / 4) //1024 bytes

int main(){
    printf("---Test iniziale del pseudo malloc---\n");

    srand(time(NULL)); //inizializzazione generatore numeri casuali per le dimensioni

    printf("Test 1: allocazione e deallocazione singola (Buddy e mmap): \n");
    void *p1_small = my_malloc(100); // Dovrebbe usare il Buddy Allocator (es. 128 byte)
    void *p2_large = my_malloc(20000); // Dovrebbe usare mmap (20 KB)
    void *p3_min_buddy = my_malloc(1); // Dovrebbe usare il Buddy Allocator (es. 64 byte)
    void *p4_threshold_minus_1 = my_malloc(MALLOC_THRESHOLD_FOR_TESTS - 1); // Dovrebbe usare Buddy (es. 1023 byte)
    void *p5_threshold_plus_1 = my_malloc(MALLOC_THRESHOLD_FOR_TESTS + 1); // Dovrebbe usare Mmap (es. 1025 byte)

    printf("   my_malloc(100)  -> %p\n", p1_small);
    printf("   my_malloc(20000) -> %p\n", p2_large);
    printf("   my_malloc(1)    -> %p\n", p3_min_buddy);
    printf("   my_malloc(1023) -> %p\n", p4_threshold_minus_1);
    printf("   my_malloc(1025) -> %p\n", p5_threshold_plus_1);
    //print_large_alloc_list();

    //BuddyAllocator_print_pool();

    // Libera i blocchi
    my_free(p1_small);
    my_free(p2_large);
    my_free(p3_min_buddy);
    my_free(p4_threshold_minus_1);
    my_free(p5_threshold_plus_1);
    printf("   Deallocazioni semplici completate.\n\n");
    //BuddyAllocator_print_pool();


    // --- Test 2: Allocazione per Riempire Parzialmente il Buddy Pool ---
    printf("2. Test Riempimento Parziale Buddy Pool:\n");
    void* buddy_blocks[NUM_RANDOM_ALLOCS / 2]; // Solo per buddy test, metà del numero totale per riempirlo parzialmente
    int buddy_count = 0;

    for (int i = 0; i < NUM_RANDOM_ALLOCS / 2; ++i) {
        size_t size = (rand() % (MALLOC_THRESHOLD_FOR_TESTS / 2)) + 1; // Richieste piccole, molto al di sotto della soglia
        buddy_blocks[buddy_count] = my_malloc(size);
        if (buddy_blocks[buddy_count]) {
            //memset per stressare la memoria
            memset(buddy_blocks[buddy_count], (i % 255) + 1, size > 16 ? 16 : size);
            buddy_count++;
        }
    }
    printf("   Allocati %d blocchi piccoli nel buddy pool.\n", buddy_count);
    //BuddyAllocator_print_bitmap();

    // Libera i blocchi nell'ordine di allocazione, per facilitare la coalescenza
    for (int i = 0; i < buddy_count; ++i) {
        if (buddy_blocks[i]) {
            my_free(buddy_blocks[i]);
        }
    }
    printf("   Deallocati blocchi piccoli\n\n");


    // --- Test 3: Stress Test con Allocazioni e Deallocazioni Casuali ---
    printf("3. Stress Test: %d Allocazioni e Deallocazioni Casuali\n", NUM_RANDOM_ALLOCS);
    void* ptrs[NUM_RANDOM_ALLOCS];
    size_t sizes[NUM_RANDOM_ALLOCS];
   
    // Fase di allocazione casuale, sia per buddy allocator che per mmap
    printf("   Fase di Allocazione...\n");
    for (int i = 0; i < NUM_RANDOM_ALLOCS; ++i) {
        sizes[i] = (rand() % MAX_RANDOM_SIZE) + 1; // Dimensione tra 1 e MAX_RANDOM_SIZE
        ptrs[i] = my_malloc(sizes[i]);
        if (ptrs[i] == NULL) {
            fprintf(stderr, "   Errore: my_malloc() ha restituito NULL per dimensione %zu (i=%d).\n", sizes[i], i);
        } else {
            // Scrive alcuni byte per verificare che la memoria sia accessibile
            memset(ptrs[i], (i % 255) + 1, sizes[i] > 16 ? 16 : sizes[i]);
        }
    }
    //print_large_alloc_list();
    //BuddyAllocator_print_bitmap();
    printf("   Allocazioni casuali completate. Iniziando le deallocazioni...\n");

    // Fase di deallocazione casuale (in ordine sparso per stressare di più)
    for (int i = 0; i < NUM_RANDOM_ALLOCS; ++i) {
        if (ptrs[i] != NULL) {
            my_free(ptrs[i]);
            ptrs[i] = NULL; // Previene double-free se si esegue il test più volte
        }
    }
    printf("   Deallocazioni casuali completate.\n\n");

    printf("Test 4: casi limite per verificare errori\n");
    void* null_ptr = my_malloc(0);
    printf("my malloc(0) -> %p (atteso NULL)\n", null_ptr);
    my_free(NULL);
    printf("my_free(NULL) chiamata\n");
    my_free((void*)0x12345678); //liberazione di un puntatore non allocato
    printf("my_free(0x12345678) chiamata (dovrebbe generare un errore su stderr)\n");
}