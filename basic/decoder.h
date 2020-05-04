#ifndef INC_DECODER_H
#define INC_DECODER_H 1

#include <stdbool.h>
#include <stdio.h>

enum Dialect
  {
   MIN_DIALECT = 0, mos6502_32000 = MIN_DIALECT,
   Z80_80x86 = 1,
   ARM = 2,
   Windows = 3,
   Mac = 4,
   NUM_DIALECTS,
  };

enum { NUM_TOKENS = 0x100 };

/* expansion_map maps an input byte either to a token, or
 * to a special string (beginning with underscore) which
 * tells us we need to perform a lookup in a second map.
 */
struct expansion_map
{
  char *mapping[NUM_TOKENS];
};

struct decoder
{
  enum Dialect dialect;
  struct expansion_map xmap;
  int listo;
};

struct decoder* new_decoder(enum Dialect d, int listo);
void destroy_decoder(struct decoder* p);
bool decode_file(struct decoder *dec, const char *filename, FILE* f);

#endif
