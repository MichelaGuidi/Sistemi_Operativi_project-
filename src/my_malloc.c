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

//struttura per memorizzare le informazioni in caso di allocazione grande
typedef struct LargeAllocInfo{
    void* ptr; //puntatore all'inizio del blocco di memoria allocato con mmap
    size_t size; //dimesione del blocco
    struct LargeAllocInfo* next; //puntatore al prossimo elemento della lista
} LargeAllocInfo;

//puntatore alla testa della lista delle allocazioni grandi (inizialmente vuota)
static LargeAllocInfo* large_allocs_head = NULL;

// funzione che aggiunge un'allocazione grande alla lista
static void add_large_alloc(void* ptr, size_t size){

    //alloca la struttura usando mmap
    LargeAllocInfo* new_node = (LargeAllocInfo*)mmap(NULL, sizeof(LargeAllocInfo), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    if (new_node == MAP_FAILED){
        //l'allocazione non può essere tracciata
        perror("Errore: fallita l'allocazione del nodo LargeAllocInfo");
        return;
    }

    //inserisce le informazioni nel nuovo nodo
    new_node->ptr = ptr;
    new_node->size = size;

    //il nuovo nodo si trova all'inizio della lista
    new_node->next = large_allocs_head;
    large_allocs_head = new_node;
}

//funzione per la rimozione di un'allocazione grande dalla lista
static int remove_large_alloc(void* ptr, size_t* out_size){
    LargeAllocInfo* current = large_allocs_head;
    LargeAllocInfo* prev = NULL;

    //ciclo che scorre la lista per trovare il nodo corrispondente a ptr
    while(current != NULL && current->ptr != ptr){
        prev = current;
        current = current->next;
    }

    //caso in cui il puntatore non venga trovato
    if (current == NULL){
        return 0;
    } 
    
    //caso in cui il puntatore sia stato trovato
    if (prev == NULL){
        //il nodo è in testa alla lista
        large_allocs_head = current->next;
    } else {
        //il nodo è in mezzo o a fine lista
        prev->next = current->next;
    }

    //dimensione originale del blocco
    if (out_size != NULL){
        *out_size = current->size;
    }

    if (munmap(current, sizeof(LargeAllocInfo)) == -1){
        perror("Errore: fallita deallocazione della memoria occupata dal nodo");
    }

    return 1;
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