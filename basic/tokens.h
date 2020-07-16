//
//   Copyright 2020 James Youngman
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
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
