#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include "afsp.h"
#include "commands.h"
#include "dfs.h"
#include "dfscontext.h"
#include "dfsimage.h"
#include "fsp.h"
#include "stringutil.h"

using std::cerr;
using std::cout;
using std::string;
using std::vector;

using DFS::byte;
using DFS::offset;

namespace 
{
  inline unsigned long crc_cycle(unsigned long crc)
  {
    if (crc & 32768)
      return  (((crc ^ 0x0810) & 32767) << 1) + 1;
    else
      return crc << 1;
  }
}  // namespace

namespace DFS
{
using stringutil::rtrim;
using stringutil::case_insensitive_equal;
using stringutil::case_insensitive_less;

  unsigned long sign_extend(unsigned long address)
  {
    /*
      The load and execute addresses are 18 bits.  The largest unsigned
      18-bit value is 0x3FFFF (or &3FFFF if you prefer).  However, the
      DFS *INFO command prints the address &3F1900 as FF1900.  This is
      because, per pages K.3-1 to K.3-2 of the BBC Master Reference
      manual part 2,

      > BASIC sets the high-order bits of the load address to the
      > high-order address of the processor it is running on.  This
      > enables you to tell if a file was saved from the I/O processor
      > or a co-processor.  For example if there was a BASIC file
      > called prog1, its information might look like this:
      >
      > prog1 FFFF0E00 FFFF8023 00000777 000023
      >
      > This indicates that prog1 was saved on an I/O processor-only
      > machine with PAGE set to &E00.  The execution address
      > (FFFF8023) is not significant for BASIC programs.
    */
    if (address & 0x20000)
      {
	// We sign-extend just two digits (unlike the example above) ,
	// as this is what the BBC model B DFS does.
	return 0xFF0000 | address;
      }
    else
      {
	return address;
      }
  }

  unsigned long compute_crc(const byte* start, const byte *end)
  {
    unsigned long crc = 0;
    for (const byte* p = start; p < end; ++p)
      {
	crc ^= *p++ << 8;
	for(int k = 0; k < 8; k++)
	  crc = crc_cycle(crc);
	assert((crc & ~0xFFFF) == 0);
      }
    return crc;
  }
}  // namespace DFS
