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
