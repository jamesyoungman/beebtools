#include "commands.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "commands.h"
#include "dfs_filesystem.h"
#include "stringutil.h"

using std::string;
using std::vector;
using std::cout;

namespace
{
  using DFS::Catalog;
  using DFS::CatalogEntry;
  using DFS::FileSystem;
  using DFS::Format;

  class CommandCat : public DFS::CommandInterface
  {
  public:
    CommandCat() {}

    const std::string name() const override
    {
      return "cat";
    }

    const std::string usage() const override
    {
      return "usage: cat [drive-number]\n"
	"If drive-number is not specified use the value from the --drive "
	"global option.\n";
    }

    const std::string description() const override
    {
      return "display the disc catalogue";
    }

    bool operator()(const DFS::StorageConfiguration& storage,
		    const DFS::DFSContext& ctx,
		    const std::vector<std::string>& args) override
    {
      unsigned int d;
      if (args.size() > 2)
	{
	  std::cerr << "Please specify at most one argument, the drive number\n";
	  return false;
	}
      else if (args.size() == 2)
	{
	  if (!DFS::StorageConfiguration::decode_drive_number(args[1], &d))
	    return false;
	}
      else
	{
	  d = ctx.current_drive;
	}
      std::unique_ptr<FileSystem> file_system = storage.mount(d);
      const Catalog& catalog(file_system->root());
      const auto ui = file_system->ui_style(ctx);
      cout << catalog.title();
      std::optional<int> cycle_count = catalog.sequence_number();
      if (cycle_count)
	{
	  // HDFS uses this field of the root catalog as a checksum
	  // instead.
	  cout << " ("  << std::setbase(16) << cycle_count.value() << ")";
	}
      cout << std::setbase(10);
      // TODO: determine whether then image is FM or MFM, print the
      // right indicator (and spell it appropriately for the UI).

      if (DFS::UiStyle::Watford == ui)
	{
	  cout << "   Single density\n";
	}
      else
	{
	  cout << " FM\n";
	}
      const auto opt = catalog.boot_setting();
      cout << "Drive "<< ctx.current_drive
	   << "             Option " << opt << "\n";
      if (DFS::UiStyle::Watford == ui)
	{
	  cout << "Directory :" << ctx.current_drive << "." << ctx.current_directory
	       << "      "
	       << "Library :0.$\n";
	  cout << "Work file $.\n";
	}
      else
	{
	  cout << "Dir. :" << ctx.current_drive << "." << ctx.current_directory
	       << "           "
	       << "Lib. :0.$\n";
	}
      cout << "\n";

      auto entries = catalog.entries();
      auto compare_entries =
	[&catalog, &ctx](const CatalogEntry& l, const CatalogEntry& r) -> bool
	{
	  // Ensure that entries in the current directory sort
	  // first.
	  auto mapdir =
	    [&ctx] (char dir) -> char {
	      if (dir == ctx.current_directory)
		return '\0';
	      return static_cast<char>(tolower(static_cast<unsigned char>(dir)));
	    };

	  if (mapdir(l.directory()) < mapdir(r.directory()))
	    return true;
	  if (mapdir(r.directory()) < mapdir(l.directory()))
	    return false;
	  // Same directory, compare names.
	  return DFS::stringutil::case_insensitive_less(l.name(), r.name());
	};

      std::sort(entries.begin(), entries.end(), compare_entries);

      bool left_column = true;
      bool printed_gap = false;
      std::cout << std::left;
      constexpr int name_col_width = 8;
      bool first = true;
      const int left_col_indent = (DFS::UiStyle::Watford == ui) ? 2 : 1;
      for (const auto& entry : entries)
	{
	  if (entry.directory() != ctx.current_directory)
	    {
	      if (!printed_gap)
		{
		  if (!first)
		    cout << (left_column ? "\n" : "\n\n");
		  left_column = true;
		  printed_gap = true;
		}
	    }
	  first = false;

	  cout << std::setw(left_column ? left_col_indent : 7) << "";
	  std::cout << std::setw(0);
	  if (entry.directory() != ctx.current_directory)
	    cout << " " << entry.directory() << ".";
	  else
	    cout << "   ";
	  cout << std::setw(0) << std::left << entry.name();
	  int acc_col_width = name_col_width - static_cast<int>(entry.name().size());
	  if (entry.is_locked() || left_column)
	    {
	      cout << std::setfill(' ') << std::setw(2 + acc_col_width)
		   << std::right << (entry.is_locked() ? "L": "") << std::setfill(' ');
	    }
	  if (!left_column)
	    {
	      cout << "\n";
	    }
	  left_column = !left_column;
	}
      if (DFS::UiStyle::Watford == ui)
	{
	  cout << "\n";
	  // TODO: Watford DFS states the number of tracks too.
	  cout << std::setw(2) << std::setfill('0') << std::right << entries.size()
	       << " files of " << catalog.max_file_count()
	       << " on 80 tracks\n";
	  // TODO: plumb in the real geometry.
	}
      return true;
    }
  };

  REGISTER_COMMAND(CommandCat);
}
