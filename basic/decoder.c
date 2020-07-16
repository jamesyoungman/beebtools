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
#include "decoder.h"

#include <stdio.h>
#include <stdlib.h>

#include "tokens.h"

struct decoder* new_decoder(enum Dialect d, int listo)
{
  struct decoder *result = malloc(sizeof(*result));
  if (NULL == result)
    return NULL;
  result->listo = listo;
  result->dialect = d;
  if (!build_mapping(d, &result->xmap))
    return NULL;
  return result;
}

void destroy_decoder(struct decoder* p)
{
  free(p);
}

static bool dialect_has_leading_cr(enum Dialect d)
{
  return (d == mos6502_32000 || d == ARM || d == Mac || d == PDP11);
}



bool decode_file(struct decoder*dec, const char *filename, FILE* f)
{
  bool (*line_decoder)(FILE *f, const char *filename,
		       const struct expansion_map* m,
		       int listo) = dialect_has_leading_cr(dec->dialect)
    ? decode_big_endian_program : decode_little_endian_program;
  return line_decoder(f, filename, &dec->xmap, dec->listo);
}
