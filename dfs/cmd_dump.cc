#include "commands.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "cleanup.h"
#include "dfstypes.h"
#include "hexdump.h"

namespace
{
  constexpr long int Stride = 8;

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

    bool invoke(const DFS::StorageConfiguration& storage,
		const DFS::DFSContext& ctx,
		const std::vector<std::string>& args) override
    {
      return body_command(storage, ctx, args,
			  [](const DFS::byte* body_start,
			     const DFS::byte *body_end,
			     const std::vector<std::string>&)
			  {
			    ostream_flag_saver restore_cout_flags(std::cout);
			    std::cout << std::hex << std::uppercase;
			    assert(body_end >= body_start);
			    const size_t len = body_end - body_start;
			    return hexdump_bytes(std::cout, 0, len, Stride, body_start);
			  });
    }
};
REGISTER_COMMAND(CommandDump);

}  // namespace DFS
