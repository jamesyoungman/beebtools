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

#include <algorithm>        // for copy
#include <iostream>         // for operator<<, basic_ostream, ostream, cerr
#include <iterator>         // for back_insert_iterator, back_inserter
#include <optional>         // for optional
#include <string>           // for string, operator<<, char_traits
#include <vector>           // for vector, vector<>::const_iterator

#include "dfs_catalog.h"    // for Catalog, CatalogEntry
#include "dfs_volume.h"     // for Volume
#include "dfstypes.h"       // for byte
#include "driveselector.h"  // for operator<<
#include "fsp.h"            // for ParsedFileName, parse_filename
#include "storage.h"        // for VolumeMountResult, StorageConfiguration

namespace DFS { class DataAccess; }
namespace DFS { struct DFSContext; }

namespace
{
  void read_file_body(const DFS::CatalogEntry& entry,
		      DFS::DataAccess& media,
		      std::vector<DFS::byte>* body)
  {
    entry.visit_file_body_piecewise(media,
				    [body]
				    (const DFS::byte* begin, const DFS::byte* end)
				    {
				      std::copy(begin, end,
						std::back_inserter(*body));
				      return true;
				    });
  }

}  // namespace


namespace DFS
{
  CommandInterface* CIReg::get_command(const std::string& name)
  {
    auto m = get_command_map();
    auto it = m->find(name);
    if (it == m->end())
      return 0;
    return it->second.get();
  }

  CIReg::map_type* CIReg::get_command_map()
  {
    static std::unique_ptr<map_type> instance;
    if (!instance)
      {
	instance = std::make_unique<map_type>();
      }
    assert(instance);
    return instance.get();
  }

  bool CIReg::visit_all_commands(std::function<bool(CommandInterface*)> visitor)
  {
    auto m = get_command_map();
    for (const auto& cmd : *m)
      {
	bool ok = visitor(cmd.second.get());
	if (!ok)
	  return false;
      }
    return true;
  }

bool body_command(const StorageConfiguration& storage, const DFSContext& ctx,
		  const std::vector<std::string>& args,
		  file_body_logic logic)
{
  if (args.size() < 2)
    {
      std::cerr << "please give a file name.\n";
      return false;
    }
  if (args.size() > 2)
    {
      // The Beeb ignores subsequent arguments.
      std::cerr << "warning: ignoring additional arguments.\n";
    }
  ParsedFileName name;
  std::string error;
  if (!parse_filename(ctx, args[1], &name, error))
    {
      std::cerr << args[1] << " is not a valid file name: " << error << "\n";
      return false;
    }
  auto mounted = storage.mount(name.vol, error);
  if (!mounted)
    {
      std::cerr << "failed to select drive " << name.vol << ": " << error << "\n";
      return false;
    }
  const auto& root(mounted->volume()->root());
  const std::optional<CatalogEntry> entry = root.find_catalog_entry_for_name(name);
  if (!entry)
    {
      std::cerr << args[1] << ": not found\n";
      return false;
    }
  std::vector<DFS::byte> body;
  DataAccess& vol_access(mounted->volume()->data_region());
  read_file_body(*entry, vol_access, &body);
  const std::vector<std::string> tail(args.begin() + 1, args.end());
  return logic(body.data(), body.data() + body.size(), tail);
}

}  // namespace DFS
