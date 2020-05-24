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
  return (d == mos6502_32000 || d == ARM || d == Mac);
}



bool decode_file(struct decoder*dec, const char *filename, FILE* f)
{
  bool (*line_decoder)(FILE *f, const char *filename,
		       const struct expansion_map* m,
		       int listo) = dialect_has_leading_cr(dec->dialect)
    ? decode_big_endian_program : decode_little_endian_program;
  return line_decoder(f, filename, &dec->xmap, dec->listo);
}
