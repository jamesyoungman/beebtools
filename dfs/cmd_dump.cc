#include "commands.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "dfstypes.h"

namespace
{
  const long int HexdumpStride = 8;

  bool hexdump_bytes(size_t pos, size_t len, const DFS::byte* data)
  {
    using std::cout;
    cout << std::setw(6) << std::setfill('0') << pos;
    for (size_t i = 0; i < HexdumpStride; ++i)
      {
	if (i < len)
	  cout << ' ' << std::setw(2) << std::setfill('0') << unsigned(data[pos + i]);
	else
	  cout << std::setw(3) << std::setfill(' ') << ' ';
      }
    cout << ' ';
    for (size_t i = 0; i < len; ++i)
      {
	const char ch = data[pos + i];
	if (isgraph(ch))
	  cout << ch;
	else
	  cout << '.';
      }
    cout << '\n';
    return true;
  }

}  // namespace

namespace DFS
{
class CommandDump : public CommandInterface // *DUMP
{
public:
  const std::string name() const override
    {
      return "dump";
    }

    const std::string usage() const override
    {
      return "usage: dump filename\n";
    }

    const std::string description() const override
    {
      return "displays the contents of a file in both hex and printable characters";
    }

    bool operator()(const DFS::StorageConfiguration& storage,
		    const DFS::DFSContext& ctx,
		    const std::vector<std::string>& args) override
    {
      return body_command(storage, ctx, args,
			  [](const DFS::byte* body_start,
			     const DFS::byte *body_end,
			     const std::vector<std::string>&)
			  {
			    std::cout << std::hex;
			    assert(body_end > body_start);
			    size_t len = body_end - body_start;
			    for (size_t pos = 0; pos < len; pos += HexdumpStride)
			      {
				auto avail = (pos + HexdumpStride > len) ? (len - pos) : HexdumpStride;

				if (!hexdump_bytes(pos, avail, body_start))
				  return false;
			      }
			    return true;
			  });
    }
};
REGISTER_COMMAND(CommandDump);

}  // namespace DFS

