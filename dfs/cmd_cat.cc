#include "commands.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "commands.h"
#include "dfsimage.h"
#include "stringutil.h"

using std::string;
using std::vector;
using std::cout;

namespace
{
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
      if (args.size() > 2)
	{
	  std::cerr << "Please specify at most one argument, the drive number\n";
	  return false;
	}
      else if (args.size() == 2)
	{
	  if (!storage.select_drive_by_afsp(args[1], &drive, ctx.current_drive))
	    return false;
	}
      else
	{
	  if (!storage.select_drive(ctx.current_drive, &drive))
	    return false;
	}
      FileSystem file_system(drive);
      const FileSystem* fs = &file_system; // TODO: this is a bit untidy

      cout << fs->title();
      std::optional<int> cycle_count = fs->cycle_count();
      if (cycle_count)
	{
	  // HDFS uses this field for something else.
	  cout << " ("  << std::setbase(16) << cycle_count.value() << ")";
	}
      cout << std::setbase(10) << " FM\n";
      const auto opt = fs->opt_value();
      cout << "Drive "<< ctx.current_drive
	   << "            Option " << opt << "\n";
      cout << "Dir. :" << ctx.current_drive << "." << ctx.current_directory
	   << "          "
	   << "Lib. :0.$\n\n";

      const unsigned short entries = fs->global_catalog_entry_count();
      vector<unsigned short int> ordered_catalog_index;
      ordered_catalog_index.reserve(entries);
      ordered_catalog_index.push_back(0);	  // dummy for title
      for (unsigned short i = 1; i <= entries; ++i)
	ordered_catalog_index.push_back(i);


      auto compare_entries =
	[&fs, &ctx](unsigned short left, unsigned short int right) -> bool
	{
	  const auto& l = fs->get_global_catalog_entry(left);
	  const auto& r = fs->get_global_catalog_entry(right);
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
	  return DFS::stringutil::case_insensitive_less
	    (fs->get_global_catalog_entry(left).name(),
	     fs->get_global_catalog_entry(right).name());
	};

      std::sort(ordered_catalog_index.begin()+1,
		ordered_catalog_index.end(),
		compare_entries);

      bool left_column = true;
      bool printed_gap = false;
      std::cout << std::left;
      constexpr int name_col_width = 8;
      for (int i = 1; i <= entries; ++i)
	{
	  auto entry = fs->get_global_catalog_entry(ordered_catalog_index[i]);
	  if (entry.directory() != ctx.current_directory)
	    {
	      if (!printed_gap)
		{
		  if (i > 1)
		    cout << (left_column ? "\n" : "\n\n");
		  left_column = true;
		  printed_gap = true;
		}
	    }

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
      cout << fs->global_catalog_entry_count() << " files of "
	   << fs->max_file_count() << "\n";
      // TODO: Watford DFS states the number of tracks too.
      return true;
    }
  };

  REGISTER_COMMAND(CommandCat);
}
