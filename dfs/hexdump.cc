#include "hexdump.h"

#include <iomanip>
#include <cctype>

#include "cleanup.h"


namespace DFS
{

bool hexdump_bytes(std::ostream& os,
		   size_t pos, size_t len, size_t stride,
		   const DFS::byte* data)
{
  ostream_flag_saver saver(os);

  while (len)
    {
      os << std::setw(6) << std::setfill('0') << std::dec << pos << std::hex;
      for (size_t i = 0; i < stride; ++i)
	{
	  if (i < len)
	    os << ' ' << std::setw(2) << std::setfill('0') << std::uppercase
	       << unsigned(data[pos + i]);
	  else
	    os << " **";
	}
      os << ' ';
      for (size_t i = 0; i < stride; ++i)
	{
	  char ch = '.';
	  if (i < len)
	    {
	      ch = data[pos + i];
	    }
	  // TODO: generate a test file which verifies that all the
	  // members of the character set are correctly characterised as
	  // being printed directly or as '.'.
	  if (ch == ' ' || isgraph(ch))
	    os << ch;
	  else
	    os << '.';
	}
      os << '\n';
      if (len >= stride)
	{
	  len -= stride;
	  pos += stride;
	}
      else
	{
	  break;
	}
    }

  return true;
}

}  // namespace DFS
