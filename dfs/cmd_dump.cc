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
#include <assert.h>    // for assert
#include <stddef.h>    // for size_t
#include <iostream>    // for cout, hex, uppercase, basic_ostream<>::__ostre...
#include <string>      // for string, allocator
#include <vector>      // for vector

#include "cleanup.h"   // for ostream_flag_saver
#include "commands.h"  // for body_command, CommandInterface, REGISTER_COMMAND
#include "dfstypes.h"  // for byte
#include "hexdump.h"   // for hexdump_bytes

namespace
{
  constexpr long int Stride = 8;

}  // namespace

namespace DFS
{
class StorageConfiguration;
struct DFSContext;

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
