#ifndef INC_TOKENS_H
#define INC_TOKENS_H 1

#include <stdio.h>
#include <stdbool.h>

#include "decoder.h"

extern const char invalid[];
extern const char line_num[];
bool build_mapping(unsigned dialect, struct expansion_map*);
void destroy_mapping(struct expansion_map*);
bool map_c6(enum Dialect, unsigned char, const char **);
bool map_c7(enum Dialect, unsigned char, const char **);
bool map_c8(enum Dialect, unsigned char, const char **);
bool set_dialect(const char* name, enum Dialect* d);
bool print_dialects(FILE*, const char *default_dialect_name);
bool decode_len_leading_program(FILE *f, const char *filename,
				enum Dialect dialect, const struct expansion_map *m, int listo);
bool decode_cr_leading_program(FILE *f, const char *filename,
			       enum Dialect dialect, const struct expansion_map *m, int listo);
void please_submit_bug_report();


#endif
