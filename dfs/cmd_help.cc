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
#include <algorithm>   // for max
#include <iomanip>     // for operator<<, setw
#include <iostream>    // for operator<<, basic_ostream, ostream, cout, left
#include <limits>      // for numeric_limits
#include <map>         // for map
#include <string>      // for operator<<, char_traits, string, allocator
#include <utility>     // for pair
#include <vector>      // for vector

#include "commands.h"  // for CommandHelp, CommandInterface, CIReg, REGISTER...
#include "dfs.h"       // for get_option_help

// CommandHelp is in the DFS namespace rather than in the unnamed
// namespace, because we need to directly instantiate it.

using std::cout;

namespace DFS
{
  class StorageConfiguration;
  struct DFSContext;

  const std::string CommandHelp::name() const
  {
    return "help";
  }

  const std::string CommandHelp::usage() const
  {
    return name() + " [command]...\n";
  }

  const std::string CommandHelp::description() const
  {
    return "explain how to use one or more commands";
  }

  bool CommandHelp::invoke(const DFS::StorageConfiguration&,
			   const DFS::DFSContext&,
			   const std::vector<std::string>& args)
  {
    const auto& option_help = get_option_help();
    const int max_command_name_len = 14;
    if (args.size() < 2)
      {
	cout << "usage: dfs [global-options] command [command-options] [command-arguments]\n"
	     << "\n"
	     << "This is a program for extracting information from Acorn DFS disc images.\n"
	     << "\n"
	     << "The global options affect almost all commands.  See below for details.\n"
	     << "The command is a single word (for example dump, info) specifying what\n"
	     << "action should be performed on one or more of the files within the DFS\n"
	     << "disc image.  The command options modify the way the command works.\n"
	     << "Most commands take no options.  The command-arguments typically specify\n"
	     << "which files within the disc image will be selected.\n"
	     << "\n"
	     << "Global options:\n";
	int max_option_len = 0;
	for (const auto& h : option_help)
	  {
	    assert(h.first.size() < std::numeric_limits<int>::max());
	    max_option_len = std::max(max_option_len,
				      static_cast<int>(h.first.size()));
	  }
	for (const auto& h : option_help)
	  {
	    cout << "--" << std::left << std::setw(max_option_len)
		 << h.first << ": " << h.second << "\n";
	  }
	cout << "\n";

	const std::string prefix = "      ";
	cout << "Commands:\n";
	auto show = [prefix, max_command_name_len](CommandInterface* c) -> bool
		    {
		      cout << prefix << std::setw(max_command_name_len)
			   << std::left << c->name() << ": "
			   << c->description() << "\n";
		      return cout.good();
		    };
	auto ok = CIReg::visit_all_commands(show);
	cout << "For help on any individual command, use \"help command-name\"\n";
	return ok && cout.good();
      }
    else
      {
	for (unsigned int i = 1; i < args.size(); ++i)
	  {
	    auto instance = CIReg::get_command(args[i]);
	    if (instance)
	      {
		cout << std::setw(args.size() > 2 ? max_command_name_len : 0)
		     << std::left << args[i]
		     << ": " << instance->description()
		     << "\n" << instance->usage() << "\n";
		if (!cout.good())
		  return false;
	      }
	    else
	      {
		std::cerr << args[i] << " is not a known command.\n";
		return false;
	      }
	  }
	return true;
      }
  }
namespace {
REGISTER_COMMAND(CommandHelp);
}  // namespace
}  // namespace DFS
