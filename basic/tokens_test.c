#include "tokens.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void verify_mapping(enum Dialect d, const struct expansion_map *m)
{
  bool missing = false;
  unsigned i;
  for (i = 0u; i < NUM_TOKENS; ++i)
    {
      if (NULL == m->base[i])
	{
	  fprintf(stderr, "missing base mapping for token 0x%02X in dialect %u\n", i, d);
	}
    }
  if (missing)
    {
      abort();
    }
}

void verify_all_mappings()
{
  unsigned int d;
  for (d = MIN_DIALECT; d < NUM_DIALECTS; ++d)
    {
      struct decoder *dec = new_decoder(d, 0);
      assert(NULL != dec);
      verify_mapping(d, &dec->xmap);
      destroy_decoder(dec);
    }
}


int main ()
{
  verify_all_mappings();
  return 0;
}
