# Pseudo Malloc - un allocatore di memoria personalizzato

## Descrizione

Questo progetto implementa un allocatore di memoria che funge da sostituto delle funzioni standard 'malloc()' e 'free()'. La memoria viene gestita quindi dinamicamente usando due strategie:

- **Buddy Allocator** -> per **allocazioni piccole** (inferiori a 1/4 della dimensione di una pagina)
- **mmap** -> per **allocazioni grandi** (uguali o superiori a 1/4 della pagina)

## Struttura del progetto
- 'src/my_malloc.c' - Implementazione principale dell'allocatore
- 'include/my_malloc.h' - Header pubblico con le funzioni esportate
- 'tests/main.c' - File di test

## Logica dell'Allocazione

### Funzioni principali

#### 'void* my_malloc(size_t size)'
Sostituto di 'malloc':
- se 'size >= PAGE_SIZE / 4' -> usa **mmap**
- altrimenti -> usa **buddy allocator**

#### 'void my_free(void* ptr)'
Sostituto di 'free':
Libera la memoria allocata precedentemente, riconoscendo il tipo di allocatore utilizzato.

#### 'static void init_malloc_system()'
Funzione di inizializzazione del sistema di allocazione:
stabilisce la grandezza di una pagina e qual è la soglia per la distinzione degli allocatori.
Inoltre chiama la funzione che inizializza il buddy allocator.

### Funzioni per allocazioni grandi

#### 'static void_large_alloc(void* ptr, size_t size)'
Questa funzione aggiunge l'oggetto puntato da ptr e della dimensione size alla lista di allocazioni grandi (struttura con elementi di tipo LargeAllocInfo)

#### 'static int remove_large_alloc(void* ptr, size_t* out_size)'
Questa funzione rimuove una grande allocazione dalla lista

### Funzioni per Buddy Allocator

#### 'static void BuddyAllocator_init()'
Inizializza il BuddyAllocator con l'uso di mmap per il pool di memoria, e memset per impostare la bitmap a 0.

#### 'static void* BuddyAllocator_malloc(size_t size)'
Funzione che occupa un blocco di memoria scegliendo tra quelli liberi. Se il blocco è troppo grande lo divide, scendendo per il figlio sinistro e impostando il bit del padre a 1. Una volta trovato imposta il suo bit a 1 e restituisce il puntatore al blocco.

#### 'static void BuddyAllocator_free(void* ptr)'
Dato il suo puntatore, trova il blocco da liberare e imposta il suo bit a 0. Comincia poi un ciclo per vedere se il suo buddy è libero, così da poterli unire eventualmente. Il ciclo continua finché non trova un buddy occupato.

#### Funzioni utili per la gestione dell'allocatore
##### 'static size_t get_block_size_from_level(int level)'
Dato il livello, calcola la dimensione dei blocchi che vi si trovano

##### 'static int get_level_from_size(size_t size)'
Data la dimensione del blocco, calcola il livello in cui si trova

##### 'static size_t get_offset_from_idx_and_level(int idx, int level)'
Calcola l'offset, dato l'indice e il livello

##### 'static int get_idx_from_offset_and_level(size_t offset, int level)'
Funzione inversa della precedente

## Test
I test si trovano in 'tests/main.c' e comprendono:
- Test 1: allocazioni di diverse dimensioni prestabilite per verificare la corretta gestione degli allocatori
- Test 2: allocazioni di piccole dimensioni per il riempimento parziale del Buddy Pool
- Test 3: tante allocazioni di dimensione casuale per stress test (caso realistico)
- Test 4: allocazioni e deallocazioni di casi limite per la gestione degli errori

## Thread Safety
Le funzioni sono **thread-safe**: viene utilizzato un 'pthread_mutex_t' per sincronizzare l'accesso al sistema di allocazione.
