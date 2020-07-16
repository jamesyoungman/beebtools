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
