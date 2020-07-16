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

#include <sstream>
#include <string>
#include <vector>

#include "dfscontext.h"
#include "dfs_volume.h"
#include "driveselector.h"

namespace
{

class CommandShowTitles : public DFS::CommandInterface
{
  public:
  const std::string name() const override
  {
    return "show-titles";
  }

  const std::string usage() const override
  {
    return name() + " [drive]...\n"
      "Show the titles of the discs in the specified drives.\n"
      "If no drive argument is specified, show all titles.\n";
  }

  const std::string description() const override
  {
    return "display disc titles";
  }

  bool show_title(const DFS::StorageConfiguration& storage,
		  const DFS::SurfaceSelector& d, std::string& error)
  {
    std::unique_ptr<DFS::FileSystem> fs(storage.mount_fs(d, error));
    if (!fs)
      return false;
    std::vector<std::optional<char>> subvolumes = fs->subvolumes();
    for (std::optional<char> sv : subvolumes)
      {
	DFS::Volume *p = fs->mount(sv, error);
	if (!p)
	  return false;
	auto vol = sv ? std::make_unique<DFS::VolumeSelector>(d, *sv) : std::make_unique<DFS::VolumeSelector>(d);
	std::cout << (*vol) << ": " << p->root().title() << "\n";
      }
    return std::cout.good();
  }

  bool invoke(const DFS::StorageConfiguration& storage,
	      const DFS::DFSContext&,
	      const std::vector<std::string>& args) override
  {
    std::string error;
    auto faild = [&error](DFS::drive_number d)
		 {
		   std::cerr << "failed to select drive " << d << ": " << error << "\n";
		   return false;
		 };
    auto fail = [&error]()
		 {
		   std::cerr << error << "\n";
		   return false;
		 };
    std::vector<DFS::SurfaceSelector> todo;
    if (args.size() > 1)
      {
	bool first = true;
	for (const std::string& arg : args)
	  {
	    if (first)
	      {
		first = false;
		continue;      // this is the command name, ignore it.
	      }
	    error.clear();
	    size_t end;
	    std::optional<DFS::SurfaceSelector> surf(DFS::SurfaceSelector::parse(arg, &end, error));
	    if (!surf)
	      return fail();
	    if (!error.empty())
	      std::cerr << "warning: " << error << "\n";
	    todo.push_back(*surf);
	  }
      }
    else
      {
	todo = storage.get_all_occupied_drive_numbers();
      }

    for (DFS::SurfaceSelector surface : todo)
      {
	if (!show_title(storage, surface, error))
	  return faild(surface);
      }
    return true;
  }

};
REGISTER_COMMAND(CommandShowTitles);

} // namespace
