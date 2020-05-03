#ifndef INC_TOKENS_H
#define INC_TOKENS_H 1

#include <stdio.h>
#include <stdbool.h>

#include "decoder.h"

enum { NUM_TOKENS = 0x100 };

extern const char invalid[];
extern const char line_num[];
char** build_mapping(unsigned dialect);
void destroy_mapping(char** p);
bool map_c6(enum Dialect, unsigned char, const char **);
bool map_c7(enum Dialect, unsigned char, const char **);
bool map_c8(enum Dialect, unsigned char, const char **);
bool set_dialect(const char* name, enum Dialect* d);
bool print_dialects(FILE*, const char *default_dialect_name);
bool decode_len_leading_program(FILE *f, const char *filename,
				enum Dialect dialect, char **token_map, int listo);
bool decode_cr_leading_program(FILE *f, const char *filename,
			       enum Dialect dialect, char **token_map, int listo);
void please_submit_bug_report();


#endif
