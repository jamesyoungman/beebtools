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
   // The intiialisation of base_map
   // relies on Mac being last.
   Mac = 4,
   NUM_DIALECTS,
  };

enum { NUM_TOKENS = 0x100 };

struct expansion_map
{
  /* base maps a byte to either a token, or to a special string
     (beginning with underscore) which tells us we need to perform a
     lookup in a second map (c6, c7 or c8).
  */
  char ascii[0x80][2];
  const char *base[NUM_TOKENS];
  const char *c6[NUM_TOKENS];
  const char *c7[NUM_TOKENS];
  const char *c8[NUM_TOKENS];
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
