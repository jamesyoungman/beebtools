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
#include <memory>
#include <string>
#include <vector>

#include "afsp.h"
#include "dfs.h"
#include "dfscontext.h"
#include "dfs_catalog.h"
#include "dfs_volume.h"
#include "storage.h"

using std::cerr;
using std::cout;


class CommandInfo : public DFS::CommandInterface // *INFO
{
public:
  const std::string name() const override
  {
    return "info";
  }

  const std::string usage() const override
  {
    return "usage: " + name() + " wildcard\n"
      "The wildcard specifies which files information should be shown for.\n"
      "To specify all files, use the wildcard #.*\n"
      "Numeric values are shown in hexadecimal (base 16).\n"
      "\n"
      "The output fields are:\n"
      "  file name\n"
      "  'L' if the file is locked, otherwise spaces\n"
      "  load address (in hex)\n"
      "  execution address (in hex)\n"
      "  file length (in hex)\n"
      "  sector within the disc at which the file is stored (in hex)\n"
      "\n"
      "Load and execution addresses are sign-extended from their actual\n"
      "18 bit length (as stored in the disc catalogue) to 24 bits.\n"
      "For example, 3F1900 becomes FF1900.\n"
      "We do this for consistency with the Acorn DFS implementation.\n"
      // Reference for the significance of the 0x20000 bit is the
      // Watford DFS manual (section 9.1 "18 BIT ADDRESSING") and the
      // Master Reference Guide.
      "When the top bits (i.e. hex 20000) are set, this signifies that the\n"
      "file was saved from the I/O processor rather than the tube\n"
      "co-processor, and should be loaded back into the same processor.\n";
  }

  const std::string description() const override
  {
    return "display information about a file (for example load address)";
  }

  bool invoke(const DFS::StorageConfiguration& storage,
	      const DFS::DFSContext& ctx,
	      const std::vector<std::string>& args) override
  {
    if (args.size() < 2)
      {
	cerr << "info: please give a file name or wildcard specifying which files "
	     << "you want to see information about.\n";
	return false;
      }
    if (args.size() > 2)
      {
	cerr << "info: please specify no more than one argument (you specified "
	     << (args.size() - 1) << ")\n";
	return false;
      }
    std::string error_message;
    std::unique_ptr<DFS::AFSPMatcher> matcher =
      DFS::AFSPMatcher::make_unique(ctx, args[1], &error_message);
    if (!matcher)
      {
	cerr << "Not a valid pattern (" << error_message << "): " << args[1] << "\n";
	return false;
      }

    const DFS::VolumeSelector vol = matcher->get_volume();
    std::string error;
    auto mounted = storage.mount(vol, error);
    if (!mounted)
      {
	std::cerr << "failed to select drive " << vol << ": " << error << "\n";
	return false;
      }
    const auto& catalog(mounted->volume()->root());

    const std::vector<DFS::CatalogEntry> entries(catalog.entries());
    for (const auto& entry : entries)
      {
#if VERBOSE_FOR_TESTS
	std::cerr << "info: directory is '" << entry.directory() << "'\n";
	std::cerr << "info: item is '" << entry.name() << "'\n";
#endif
	if (!matcher->matches(vol, entry.directory(), entry.name()))
	  continue;
	cout << entry << "\n";
      }
    return true;
  }
};
REGISTER_COMMAND(CommandInfo);
