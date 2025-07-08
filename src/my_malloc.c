#include "../include/my_malloc.h"

#include <unistd.h> //per sysconf
#include <sys/mman.h> // per mmap e munmap
#include <stdio.h> //per perror
#include <pthread.h> // per pthread_mutex_t
#include <errno.h> //per perror
#include <math.h> //per log2
#include <string.h> //per memset

// inizializzazione variabili globali
static size_t PAGE_SIZE = 0; //dimensione della pagina di memoria (0 inizialmente per lazy init)
static size_t MALLOC_TRESHOLD = 0; // soglia per decidere tra mmap e buddy allocator

static pthread_mutex_t my_malloc_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex globale per thread-safety

//dichiarazione inizializzazione buddy allocator
static void BuddyAllocator_init();

// funzione inizializzazione del sistema di allocazione
static void init_mallloc_system(){
    //blocco il mutex per assicurare che l'inizializzazione avvenga una volta sola, anche se più
    //thread provano a chiamare my_malloc contemporaneamente
    pthread_mutex_lock(&my_malloc_mutex);

    if (PAGE_SIZE == 0){
        PAGE_SIZE = sysconf(_SC_PAGESIZE); //dimensione pagina di sistema
        MALLOC_TRESHOLD = PAGE_SIZE/4; //calcolo della soglia

        BuddyAllocator_init();
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
#define GET_BYTE(idx) (buddy_bitmap[(idx) / 8]) //prende il byte in cui si trova il bit all'idx
#define GET_BIT_OFFSET(idx) ((idx) % 8) //calcola la posizione del bit all'interno del byte

#define SET_BIT(idx) (GET_BYTE(idx) |= (1 << GET_BIT_OFFSET(idx))) //imposta il bit di idx a 1
#define CLEAR_BIT(idx) (GET_BYTE(idx) &= ~(1 << GET_BIT_OFFSET(idx))) //azzera il bit
#define IS_BIT_SET(idx) ((GET_BYTE(idx) >> GET_BIT_OFFSET(idx)) & 1) //controlla se il bit è 1 o 0

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
    size_t block_num_at_level = idx - ((1 << level) - 1); //1<<level = sposta 1 di level posizioni (2^level)

    //l'offset è il numero di blocco per la dimensione di un blocco a quel livello
    return block_num_at_level * get_block_size_from_level(level);
}

//funzione inversa della precedente
static int get_idx_from_offset_and_level(size_t offset, int level){
    //quanti blocchi di quel livello ci sono prima di offset
    size_t block_num_at_level = offset/get_block_size_from_level(level);

    return ((1 << level) - 1) + block_num_at_level;
}

//Funzioni per allocazioni piccole
//inizializzazione del buddy allocator
static void BuddyAllocator_init(){
    //alloca 1MB di memoria per il pool del buddy allocator usando mmap
    buddy_pool_start = (char*)mmap(NULL, BUDDY_POOL_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    
    if(buddy_pool_start == MAP_FAILED){
        perror("Errore: fallita l'allocazione del pool del buddy allocator");
        return;
    }
    //inizializza la bitmap a zero, perché completamente libero
    memset(buddy_bitmap, 0, BITMAP_SIZE_BYTES);

    //printf("buddy allocator: pool inizializzato a %p, dimensione %u byte. Bitmap di %zu byte\n", buddy_pool_start, BUDDY_POOL_SIZE, BITMAP_SIZE_BYTES);
}

//funzione che alloca un blocco di memoria dal pool del buddy allocator
static void* BuddyAllocator_malloc(size_t size){
    //alloco uno spazio all'inizio del blocco per memorizzare la sua dimensione
    size_t required_size_with_header = size + sizeof(size_t);

    //controllo se la richiesta è troppo grande
    if (required_size_with_header > BUDDY_POOL_SIZE){
        return NULL;
    }

    //determino il livello dell'albero buddy che può ospitare la dimensione richiesta
    int target_level = get_level_from_size(required_size_with_header);

    int found_idx = -1;
    int actual_level = -1;

    //scorre i livelli dell'albero dal più grande al livello target
    for (int current_level = 0; current_level <= MAX_LEVEL; ++current_level){
        //calcola l'indice di partenza e il numero di nodi per il livello corrente
        size_t level_start_idx = (1 << current_level) - 1;
        size_t num_nodes_at_level = (1 << current_level);

        for (size_t i = 0; i < num_nodes_at_level; ++i){
            int idx = level_start_idx + i;
            
            //se il bit non è impostato il blocco è libero
            if (!IS_BIT_SET(idx)){
                //controllo se la dimensione del blocco è sufficiente
                if (get_block_size_from_level(current_level) >= required_size_with_header){
                    found_idx = idx; //trovato un blocco adatto
                    actual_level = current_level; //registro il livello

                    goto found_block; //esco dal ciclo una volta trovato
                }
            }
        }
    }
    found_block:
    //se non è stato trovato un blocco adatto, restituisco null
        if (found_idx == -1){
            return NULL;
        }
        //se il blocco è troppo grande, viene diviso ricorsivamente fino alla dimensione necessaria
        while(actual_level < target_level){
            //indico che non è più libero
            SET_BIT(found_idx);
            //passa al figlio sinistro
            found_idx = LEFT_CHILD(found_idx);
            actual_level++; //scendo di livello
        }

        //indico il blocco come occupato
        SET_BIT(found_idx);
        //calcolo l'indirizzo di memoria all'interno del pool
        char* actual_block_start = buddy_pool_start + get_offset_from_idx_and_level(found_idx, actual_level);
        //memorizzo la dimensione allocata
        *(size_t*) actual_block_start = required_size_with_header;
        //restituisco il puntatore(dopo l'intestazione)
        return (void*)(actual_block_start + sizeof(size_t));
}

//implementazione del buddy_free
//libera un blocco di memoria precedentemente allocato dal pool del buddy allocator
static void BuddyAllocator_free(void* ptr){
    //puntatore all'inizio del blocco allocato
    char* original_alloc_ptr = (char*)ptr - sizeof(size_t);
    //dimensione originale del blocco (inclusa intestazione)
    size_t allocated_size = *(size_t*) original_alloc_ptr;
    //calcolo l'offset del blocco all'interno del pool di memoria
    size_t offset = (size_t)(original_alloc_ptr - buddy_pool_start);
    //livello originale del blocco in base alla dimensione
    int level = get_level_from_size(allocated_size);
    //indice del nodo corrispondente nella bitmap
    int idx = get_idx_from_offset_and_level(offset, level);

    CLEAR_BIT(idx); //libero il blocco, imposto bit a 0
    
    //ciclo che tenta di unire il blocco liberato con il suo buddy
    while(level > 0){
        int buddy_idx = BUDDY(idx); //indice del blocco buddy

        //controllo se il buddy è fuori dai limiti dell'albero
        if (buddy_idx < 0 || buddy_idx >= TOTAL_NODES){
            break;
        }

        //se il buddy è occupato, esco dal ciclo
        if (IS_BIT_SET(buddy_idx)){
            break;
        }

        CLEAR_BIT(buddy_idx); //il suo bit viene azzerato, perche non è più un'entità libera

        idx = PARENT(idx); 

        CLEAR_BIT(idx); // anche il genitore diventa libero

        level--;
    }
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
        ptr = BuddyAllocator_malloc(size);
        if (ptr == NULL){
            fprintf(stderr, "Errore: non è stato possibile usare il buddy allocator\n");
        }
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
        char* original_alloc_ptr = (char*)ptr - sizeof(size_t);
        //controllo se il puntatore rientra nel range del pool di buddy allocator
        if (buddy_pool_start != NULL && original_alloc_ptr >= buddy_pool_start && original_alloc_ptr < buddy_pool_start + BUDDY_POOL_SIZE){
            BuddyAllocator_free(ptr);
        } else {
            //se il puntatore non è stato trovato in nessuno dei due casi, allora non è gestito
            fprintf(stderr, "Tentativo di liberare un puntatore non gestito o già liberato: %p\n", ptr);
        }
    }

    pthread_mutex_unlock(&my_malloc_mutex); //sblocco il mutex
}

//funzione di debug per stampare la lista delle allocazioni grandi
void print_large_alloc_list(){
    LargeAllocInfo* current = large_allocs_head;
    printf("Stato della lista di allocazioni grandi:\n");

    if(!current){
        printf(" lista vuota \n");
        return;
    }
    int i = 0;
    while(current){
        printf("[%d] indirizzo: %p, size: %zu bytes\n", i, current->ptr, current->size);
        current = current->next;
        i++;
    }
}

//funzione di debug per stampare lo stato della bitmap del buddy allocator
void BuddyAllocator_print_bitmap(){
    printf("Stato Buddy bitmap: \n");
    for (int i = 0; i < TOTAL_NODES; ++i){
        printf("%d", IS_BIT_SET(i));
        if ((i + 1) % 64 == 0) printf("\n");
    }
    printf("\n");
}