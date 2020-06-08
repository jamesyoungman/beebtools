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
      DFS::AbstractDrive *drive;
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
      if (!storage.select_drive(d, &drive))
	return false;

      FileSystem file_system(drive);
      const Catalog& catalog(file_system.root());

      cout << catalog.title();
      std::optional<int> cycle_count = catalog.sequence_number();
      if (cycle_count)
	{
	  // HDFS uses this field of the root catalog as a checksum
	  // instead.
	  cout << " ("  << std::setbase(16) << cycle_count.value() << ")";
	}
      // TODO: determine whether then image is FM or MFM, print the
      // right indicator (and spell it appropriately for the UI).
      cout << std::setbase(10) << " FM\n";
      const auto opt = catalog.boot_setting();
      cout << "Drive "<< ctx.current_drive
	   << "            Option " << opt << "\n";
      cout << "Dir. :" << ctx.current_drive << "." << ctx.current_directory
	   << "          "
	   << "Lib. :0.$\n";
      if (file_system.ui_style(ctx) == DFS::UiStyle::Watford)
	{
	  cout << "Work file $.\n";
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

	  cout << std::setw(left_column ? 1 : 6) << "";
	  std::cout << std::setw(0);
	  if (entry.directory() != ctx.current_directory)
	    cout << " " << entry.directory() << ".";
	  else
	    cout << "   ";
	  cout << std::setw(name_col_width) << std::left << entry.name();
	  cout << std::setw(2) << std::right << (entry.is_locked() ? "L": "");
	  if (!left_column)
	    {
	      cout << "\n";
	    }
	  left_column = !left_column;
	}
      cout << "\n";
      if (file_system.ui_style(ctx) == DFS::UiStyle::Watford)
	{
	  // TODO: Watford DFS states the number of tracks too.
	  cout << entries.size() << " files of "
	       << catalog.max_file_count() << "\n";
	}
      return true;
    }
  };

  REGISTER_COMMAND(CommandCat);
}
