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
#include <optional>    // for std::optional
#include <string>      // for string, allocator
#include <vector>      // for vector

#include "cleanup.h"   // for ostream_flag_saver
#include "commands.h"  // for body_command, CommandInterface, REGISTER_COMMAND
#include "dfstypes.h"  // for byte
#include "hexdump.h"   // for hexdump_bytes
#include "storage.h"   // for AbstractDrive

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
  ~CommandDump() override {}
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
			    return hexdump_bytes(std::cout, 0, Stride, body_start, body_end);
			  });
    }
};
REGISTER_COMMAND(CommandDump);


std::optional<long int> get_arg(const std::string& which_arg,
				const std::string& the_arg,
				const long int upper_limit)
{
  long int n;
  try
    {
      size_t end;
      n = std::stol(the_arg, &end, 10);
      if (end < the_arg.size())
	{
	  std::cerr << which_arg << " " << the_arg
		    << " should not have a non-numeric "
		    << "suffix\n";
	  return std::nullopt;
	}
      else if (0 == end)
	{
	  std::cerr << which_arg << " " << the_arg
		    << " should not be empty.\n";
	  return std::nullopt;
	}
      else if (n <= upper_limit)
	{
	  return n;
	}
    }
  catch (std::out_of_range& e)
    {
      // Report failure as for n > upper_limit.
    };
  std::cerr << which_arg << " " << the_arg
	    << " should be between 0 and (for this disc) "
	    << upper_limit << " inclusive\n";
  return std::nullopt;
}

class CommandDumpSector : public CommandInterface
{
public:
  ~CommandDumpSector() override {}
  const std::string name() const override
    {
      return "dump-sector";
    }

    const std::string usage() const override
    {
      return "usage: dump-sector SIDE-NUM TRACK-NUM SECTOR-NUM\n";
    }

    const std::string description() const override
    {
      return "displays the contents of a sector in both hex and printable characters";
    }

    bool invoke(const DFS::StorageConfiguration& storage,
		const DFS::DFSContext&,
		const std::vector<std::string>& args) override
    {
      ostream_flag_saver restore_cout_flags(std::cout);
      std::string error;
      auto fail = [&error]()
		  {
		    std::cerr << error << "\n";
		    return false;
		  };

      if (args.size() != 4)
	{
	  std::cerr << usage();
	  return false;
	}

      // Decode the drive number and obtain the device geometry.
      size_t end = 0;
      // We want to select a surface instead of a volume, as it
      // doesn't make sense to request track 20 sector 3 of a
      // volume. IOW, physical sector addresses are only useful at the
      // physical media layer.
      auto surface = DFS::SurfaceSelector::parse(args[1], &end, error);
      if (!surface)
	{
	  return fail();
	}
      DFS::AbstractDrive *drive = 0;
      if (!storage.select_drive(*surface, &drive, error))
	return fail();
      const Geometry& geom = drive->geometry();

      // Decode the track and sector number
      auto track = get_arg("track", args[2], geom.cylinders-1);
      if (!track)
	return false;
      auto sector = get_arg("sector", args[3], geom.sectors-1);
      if (!sector)
	return false;

      // Load and display the sector data
      const sector_count_type sec_addr = ((*track) * geom.sectors) + (*sector);
      auto got = drive->read_block(sec_addr);
      if (!got)
	{
	  std::cerr << "error: failed to read sector at track "
		    << *track << ", sector " << *sector << "\n";
	  return false;
	}
      return hexdump_bytes(std::cout, 0, Stride, got->data(), got->data()+got->size());
    }
};
REGISTER_COMMAND(CommandDumpSector);


}  // namespace DFS
