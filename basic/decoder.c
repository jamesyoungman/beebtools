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
  result->token_map = build_mapping(d);
  if (NULL == result->token_map)
    return NULL;
  return result;
}

void destroy_decoder(struct decoder* p)
{
  destroy_mapping(p->token_map);
  free(p);
}

static bool dialect_has_leading_cr(enum Dialect d)
{
  return (d == mos6502_32000 || d == ARM || d == Mac);
}



bool decode_file(struct decoder*dec, const char *filename, FILE* f)
{
  bool (*line_decoder)(FILE *f, const char *filename,
		       enum Dialect dialect, char **token_map,
		       int listo) = dialect_has_leading_cr(dec->dialect)
    ? decode_cr_leading_program : decode_len_leading_program;
  return line_decoder(f, filename, dec->dialect, dec->token_map, dec->listo);
}

