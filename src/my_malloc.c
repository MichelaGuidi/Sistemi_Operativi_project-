#include "../include/my_malloc.h"

#include <unistd.h> //per sysconf
#include <sys/mman.h> // per mmap e munmap
#include <stdio.h> //per perror
#include <pthread.h> // per pthread_mutex_t
#include <errno.h> //per perror
#include <math.h> //per log2

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

//ALLOCAZIONI GRANDI
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
        perror("Errore: fallita l'allocazione del nodo LargeAllocInfo\n");
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
        perror("Errore: fallita deallocazione della memoria occupata dal nodo\n");
    }

    return 1;
}

//ALLOCAZIONI CON BUDDY ALLOCATOR
//definizioni costanti
#define BUDDY_POOL_SIZE (1024*1024) //dimensione totale del pool gestito dal buddy (1MB)
#define MIN_BLOCK_SIZE 64 //dimensione minima allocabile dal buddy

//definizione del numero di livelli nell'albero (livello 0: 1MB, livello MAX_LEVEL - 1: blocchi da MIN_BLOCK_SIZE)
#define MAX_LEVEL 14 //(int)(log2(BUDDY_POOL_SIZE) - log2(MIN_BLOCK_SIZE))

//definizione numero di nodi nell'albero binario (2^(MAX_LEVEL+1) - 1)
#define TOTAL_NODES ((1 << (MAX_LEVEL + 1)) - 1)

//dimensione della bitmap in byte
#define BITMAP_SIZE_BYTES ((TOTAL_NODES / 8) + (TOTAL_NODES % 8 != 0 ? 1 : 0))

static char* buddy_pool_start = NULL; //puntatore all'inizio del pool di memoria gestito dal buddy

static unsigned char buddy_bitmap[BITMAP_SIZE_BYTES]; //bitmap che contiene i bit che indicano lo stato dei bloccchi

//macro per la gestione dei bit della bitmap
#define GET_BYTE(idx) (buddy_bitmap[(idx) / 8])
#define GET_BIT_OFFSET(idx) ((idx) % 8)

#define SET_BIT(idx) (GET_BYTE(idx) |= (1 << GET_BIT_OFFSET(idx)))
#define CLEAR_BIT(idx) (GET_BYTE(idx) &= ~(1 << GET_BIT_OFFSET(idx)))
#define IS_BIT_SET(idx) ((GET_BYTE(idx) >> GET_BIT_OFFSET(idx)) & 1)

//funzioni per la navigazione dell'albero
#define PARENT(idx) (((idx) - 1) / 2)
#define LEFT_CHILD(idx) (2 * (idx) + 1)
#define RIGHT_CHILD(idx) (2 * (idx) + 2)

#define BUDDY(idx) (((idx) % 2 == 0) ? ((idx) - 1) : ((idx) + 1)) //indice del buddy di un nodo (se è sinistro restituisce il destro, altrimenti il sinistro)

//funzioni èer conversioni buddy

//calcola la dimensione del blocco di memoria corrispondente a un dato livello
static size_t get_block_size_from_level(int level){
    return BUDDY_POOL_SIZE >> level; //BUDDY_POOL_SIZE / (2^level)
}

//calcola il livello in cui si trova un blocco di una certa dimensione
static int get_level_from_size(size_t size){
    if (size < MIN_BLOCK_SIZE){
        size = MIN_BLOCK_SIZE;
    }

    //trova la minima potenza di 2 maggiore o uguale a size
    size_t actual_size = 1;
    while(actual_size < size){
        actual_size <<= 1;
    }

    //limita la dimensione al massimo del pool
    if (actual_size > BUDDY_POOL_SIZE){
        actual_size = BUDDY_POOL_SIZE;
    }

    //calcola il livello
    int level = 0;
    size_t temp_size = BUDDY_POOL_SIZE;
    while (temp_size > actual_size && level < MAX_LEVEL){
        temp_size >>= 1;
        level++;
    }
    return level;
}

//calcola l'offset all'interno di buddy_pool_start
//traduce l'indice logico nella sua posizione fisica nel pool di memoria
static size_t get_offset_from_idx_and_level(int idx, int level){
    //quanti blocchi di quel livello ci sono prima di idx
    size_t block_num_at_level = idx - ((1 << level) - 1);

    //l'offset è il numero di blocco per la dimensione di un blocco a quel livello
    return block_num_at_level * get_block_size_from_level(level);
}

//funzione inversa della precedente
static int get_idx_from_offset_and_level(size_t offset, int level){
    //quanti blocchi di quel livello ci sono prima di offset
    size_t block_num_at_level = offset/get_block_size_from_level(level);

    return ((1 << level) - 1) + block_num_at_level;
}

//implementazione della mia versione di malloc
void* my_malloc(size_t size){

    init_mallloc_system(); //controllo se il sistema è inizializzato

    //caso di richiesta 0 byte
    if (size == 0){
        return NULL;
    }

    pthread_mutex_lock(&my_malloc_mutex); //blocco il mutex

    void* ptr = NULL; //definizione puntatore che deve restituire la funzione

    //decisione di quale allocatore usare in base alla dimensione
    if (size >= MALLOC_TRESHOLD){
        //caso grande, mmap viene usata direttamente
        ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

        if (ptr == MAP_FAILED){
            perror("Errore: fallimento durante la mappatura\n");
            ptr = NULL;
        } else {
            //se ha successo, traccia l'allocazione
            add_large_alloc(ptr, size);
        }
    } else {
        //buddy allocator non ancora implementato
        fprintf(stderr, "Richiesta di allocazione piccola, usare buddy allocator\n");
    }

    pthread_mutex_unlock(&my_malloc_mutex); //sblocco il mutex

    return ptr; // restituisco il puntatore
}

//implementazione della mia versione di free
void my_free(void* ptr){

    //caso in cui il puntatore sia null
    if (ptr == NULL){
        return;
    }

    init_mallloc_system(); //controllo che il sistema sia inizializzato

    pthread_mutex_lock(&my_malloc_mutex); //blocco il mutex

    size_t freed_size; //variabile per memorizzare la dimensione del blocco liberato

    //prova a rimuovere il puntatore dalle allocazioni grandi, altrimenti utilizza buddy allocator
    if (remove_large_alloc(ptr, &freed_size)){
        //se è stato trovato e rimosso, posso fare munmap
        if (munmap(ptr, freed_size) == -1){
            perror("Errore: fallimento durante munmap\n");
        }
    } else {
        //buddy allocator non ancora implementato
        fprintf(stderr, "puntatore %p non trovato tra le allocazioni grandi.\n", ptr);
    }

    pthread_mutex_unlock(&my_malloc_mutex); //sblocco il mutex
}