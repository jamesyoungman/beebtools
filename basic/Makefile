CFLAGS = -Wall -Wextra -ggdb
TARGETS = tokens_test bbcbasic_to_text
RM = rm -f

all: $(TARGETS)

tokens.o: tokens.c tokens.h
tokens_test.o: tokens_test.c tokens.h
tokens_test: tokens_test.o tokens.o

check: check_tokens check_golden

check_tokens: tokens_test
	./tokens_test

check_golden: bbcbasic_to_text run_golden_tests.sh
	sh run_golden_tests.sh ./bbcbasic_to_text testdata

bbcbasic_to_text: bbcbasic_to_text.o tokens.o
	$(CC) $(CFLAGS) -o $@ bbcbasic_to_text.o tokens.o

clean:
	$(RM) $(TARGETS) bbcbasic_to_text.o tokens.o tokens_test.o
