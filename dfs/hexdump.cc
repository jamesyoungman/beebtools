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
