# Pseudo Malloc - un allocatore di memoria personalizzato

## Descrizione

Questo progetto implementa un allocatore di memoria che funge da sostituto delle funzioni standard 'malloc()' e 'free()'. La memoria viene gestita quindi dinamicamente usando due strategie:

- **Buddy Allocator* -> per **allocazioni piccole** (inferiori a 1/4 della dimensione di una pagina)
- **mmap** -> per **allocazioni grandi** (uguali o superiori a 1/4 della pagina)

## Struttura del progetto
- 'my_malloc.c' - Implementazione principale dell'allocatore
- 'my_malloc.h' - Header pubblico con le funzioni esportate
- 'main.c' - File di test

## Logica dell'Allocazione

### Funzioni principali

#### 'void* my_malloc(size_t size)'
Sostituto di 'malloc':
- se 'size >= PAGE_SIZE / 4' -> usa **mmap**
- altrimenti -> usa **buddy allocator**

#### 'void my_free(void* ptr)'
Sostituto di 'free':
Libera la memoria allocata precedentemente, riconoscendo il tipo di allocatore utilizzato

## Test
I test si trovano in 'tests/main.c' e comprendono:
- Test 1: allocazioni di diverse dimensioni prestabilite per verificare la corretta gestione degli allocatori
- Test 2: allocazioni di piccole dimensioni per il riempimento parziale del Buddy Pool
- Test 3: tante allocazioni di dimensione casuale per stress test (caso realistico)
- Test 4: allocazioni e deallocazioni di casi limite per la gestione degli errori

## Thread Safety
Le funzioni sono **thread-safe**: viene utilizzato un 'pthread_mutex_t' per sincronizzare l'accesso al sistema di allocazione.