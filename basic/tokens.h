#ifndef INC_TOKENS_H
#define INC_TOKENS_H 1

#include <stdio.h>
#include <stdbool.h>

#include "decoder.h"

extern const char invalid[];
extern const char line_num[];
extern const char fastvar[];
extern const char pdp_c8[];
bool build_mapping(unsigned dialect, struct expansion_map*);
void destroy_mapping(struct expansion_map*);
void build_map_c6(enum Dialect d, const char **output);
void build_map_c7(enum Dialect d, const char **output);
void build_map_c8(enum Dialect d, const char **output);
bool is_fastvar(unsigned int i);
bool set_dialect(const char* name, enum Dialect* d);
bool print_dialects(FILE*, const char *default_dialect_name);
bool decode_little_endian_program(FILE *f, const char *filename,
				  const struct expansion_map *m, int listo);
bool decode_big_endian_program(FILE *f, const char *filename,
			       const struct expansion_map *m, int listo);
void please_submit_bug_report();
/* internal_dump_all_dialects dumps the known tokens to the specified file.
 * The file format is not guaranteed to remain stable.
 */
bool internal_dump_all_dialects(const char *file_name);


#endif
