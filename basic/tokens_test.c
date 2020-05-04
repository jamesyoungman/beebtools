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
      if (NULL == m->mapping[i])
	{
	  fprintf(stderr, "missing mapping for token 0x%02X in dialect %u\n", i, d);
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
      struct expansion_map *m = calloc(1u, sizeof(*m));
      assert(NULL != m);
      assert(build_mapping(d, m));
      assert(m != NULL);
      verify_mapping(d, m);
      destroy_mapping(m);
      free(m);
    }
}


int main ()
{
  verify_all_mappings();
  return 0;
}
