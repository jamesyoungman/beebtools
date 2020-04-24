CFLAGS = -Wall -Wextra -ggdb

all: tokens_test bbcbasic_to_text

tokens.o: tokens.c tokens.h
tokens_test.o: tokens_test.c tokens.h
tokens_test: tokens_test.o tokens.o

check: tokens_test
	./tokens_test

bbcbasic_to_text: bbcbasic_to_text.o tokens.o
	$(CC) $(CFLAGS) -o $@ bbcbasic_to_text.o tokens.o
