#include "my_malloc.h"

#include <stdio.h> //per printf()
#include <stdlib.h> // per EXIT_SUCCESS, EXIT_FAILURE

int main(){
    printf("---Test iniziale del pseudo malloc---\n");
    printf("verifico la compilazione e il linking delle funzioni stub. \n");

    size_t test_size = 100; //test di allocazione blocco di memoria di grandezza 100
    void* ptr = my_malloc(test_size);

    if (ptr == NULL){
        printf("my_malloc(%zu) ha restituito NULL come previsto\n", test_size);
    } else {
        printf("my_malloc(%zu) ha restituito un puntatore non-NULL: %p (non previsto in questa fase)\n", test_size, ptr);
    }

    //tenta di liberare il puntatore
    my_free(ptr);
    printf("my_free(%p) è stata chiamata.\n", ptr);

    printf("\n---test iniziale completato con successo---\n");
    return EXIT_SUCCESS;
}