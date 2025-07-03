#include "../include/my_malloc.h"

#include <unistd.h> //per sysconf
#include <sys/mman.h> // per mmap e munmap
#include <stdio.h> //per perror
#include <pthread.h> // per pthread_mutex_t

// inizializzazione variabili globali
static size_t PAGE_SIZE = 0; //dimensione della pagina di memoria (0 inizialmente per lazy init)
static size_t MALLOC_TRESHOLD = 0; // soglia per decidere tra mmap e buddy allocator

static pthread_mutex_t my_malloc_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex globale per thread-safety

// funzione inizializzazione del sistema di allocazione
static void init_mallloc_system(){
    //blocco il mutex per assicurare che l'inizializzazione avvenga una volta sola, anche se più
    //thread provano a chiamare my_malloc contemporaneamente
    pthread_mutex_lock(&my_malloc_mutex);

    if (PAGE_SIZE == 0){
        PAGE_SIZE = sysconf(_SC_PAGESIZE); //dimensione pagina di sistema
        MALLOC_TRESHOLD = PAGE_SIZE/4; //calcolo della soglia

    }
    //sblocco il mutex
    pthread_mutex_unlock(&my_malloc_mutex);
}

//implementazione della mia versione di malloc
void* my_malloc(size_t size){

    init_mallloc_system(); //controllo se il sistema è inizializzato

    pthread_mutex_lock(&my_malloc_mutex); //blocco il mutex
    (void)size; //cast per evitare warning

    pthread_mutex_unlock(&my_malloc_mutex); //sblocco il mutex

    return NULL; // non alloco nulla per il momento, devo solo provare il codice
}

//implementazione della mia versione di free
void my_free(void* ptr){

    init_mallloc_system(); //controllo che il sistema sia inizializzato

    pthread_mutex_lock(&my_malloc_mutex); //blocco il mutex
    (void)ptr; //cast per evitare warning

    pthread_mutex_unlock(&my_malloc_mutex); //sblocco il mutex
}