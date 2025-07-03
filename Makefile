#Nome dell'eseguibile di test che verrà creato
TARGET_TEST = tests/main

# compilatore e flag 
# -Wall abilita tutti gli avvisi comuni
# -Wextra abilita gli avvisi aggiuntivi
# -g include informazioni di debug
# -I./include dice al compilatore di cercare i file .h nella directory 'include'
CC = gcc
CFLAGS = -Wall -Wextra -g -I./include

# file sorgenti della libreria e dei test
SRCS_LIB = src/my_malloc.c
SRCS_TEST = tests/main.c

# file oggetto creati dopo la compilazione
OBJS_LIB = $(SRCS_LIB:.c=.o)
OBJS_TEST = $(SRCS_TEST:.c=.o)

#regola per compilare e linkare l'eseguibile di test
all: $(TARGET_TEST)

# regola per la compilazione dei test
$(TARGET_TEST): $(OBJS_TEST) $(OBJS_LIB)
	$(CC) $(CFLAGS) $(OBJS_TEST) $(OBJS_LIB) -o $@

#regola per compilare i file sorgenti della libreria
$(OBJS_LIB): src/%.c 
	$(CC) $(CFLAGS) -c $< -o $@

#regola per compilare i file sorgenti dei test
$(OBJS_TEST): tests/%.c 
	$(CC) $(CFLAGS) -c $< -o $@

#regola per rimuovere i file compilati
clean: 
	rm -f $(OBJS_LIB) $(OBJS_TEST) $(TARGET_TEST)