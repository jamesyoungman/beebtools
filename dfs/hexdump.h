#include "dfstypes.h"

#include <cstdlib>		/* size_t */
#include <iostream>

namespace DFS
{
  bool hexdump_bytes(std::ostream&, size_t pos, size_t len, size_t stride,
		     const DFS::byte* data);
}
