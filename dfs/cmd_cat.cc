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
  using DFS::FileSystemImage;
  using DFS::Format;

  inline std::string decode_opt(DFS::byte opt)
  {
    switch (opt)
      {
      case 0: return "off";
      case 1: return "load";
      case 2: return "run";
      case 3: return "exec";
      }
    return "?";
  }

  class CommandCat : public DFS::CommandInterface
  {
  public:
    CommandCat() {}
    static CommandCat* NewInstance()
    {
      return new CommandCat;
    }

    const std::string name() const override
    {
      return "cat";
    }

    const std::string usage() const override
    {
      return "cat [drive-number]\n";
    }

    bool operator()(const DFS::StorageConfiguration& storage,
		    const DFS::DFSContext& ctx,
		    const std::vector<std::string>& args) override
    {
      const FileSystemImage *image;
      if (args.size() > 2)
	{
	  std::cerr << "Please specify at most one argument, the drive number\n";
	  return false;
	}
      else if (args.size() == 2)
	{
	  if (!storage.select_drive_by_afsp(args[1], &image, ctx.current_drive))
	    return false;
	}
      else
	{
	  if (!storage.select_drive(ctx.current_drive, &image))
	    return false;
	}
      assert(image != 0);

      cout << image->title();
      if (image->disc_format() != Format::HDFS)
	{
	  cout << " ("  << std::setbase(16) << image->cycle_count()
	       << std::setbase(10) << ") FM";
	}
      cout << "\n";
      const auto opt = image->opt_value();
      cout << "Drive "<< ctx.current_drive
	   << "            Option "
	   << opt << " (" << decode_opt(opt) << ")\n";
      cout << "Dir. :" << ctx.current_drive << "." << ctx.current_directory
	   << "          "
	   << "Lib. :0.$\n\n";

      const int entries = image->catalog_entry_count();
      vector<int> ordered_catalog_index;
      ordered_catalog_index.reserve(entries);
      ordered_catalog_index.push_back(0);	  // dummy for title
      for (int i = 1; i <= entries; ++i)
	ordered_catalog_index.push_back(i);


      auto compare_entries =
	[&image, &ctx](int left, int right) -> bool
	{
	  const auto& l = image->get_catalog_entry(left);
	  const auto& r = image->get_catalog_entry(right);
	  // Ensure that entries in the current directory sort
	  // first.
	  auto mapdir =
	    [&ctx] (char dir) -> char {
	      return dir == ctx.current_directory ? 0 : tolower(dir);
	    };

	  if (mapdir(l.directory()) < mapdir(r.directory()))
	    return true;
	  if (mapdir(r.directory()) < mapdir(l.directory()))
	    return false;
	  // Same directory, compare names.
	  return DFS::stringutil::case_insensitive_less
	    (image->get_catalog_entry(left).name(),
	     image->get_catalog_entry(right).name());
	};

      std::sort(ordered_catalog_index.begin()+1,
		ordered_catalog_index.end(),
		compare_entries);

      bool left_column = true;
      bool printed_gap = false;
      for (int i = 1; i <= entries; ++i)
	{
	  auto entry = image->get_catalog_entry(ordered_catalog_index[i]);
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

	  if (!left_column)
	    {
	      cout << std::setw(6) << "";
	    }

	  cout << " ";
	  if (entry.directory() != ctx.current_directory)
	    cout << " " << entry.directory() << ".";
	  else
	    cout << "   ";
	  cout << entry.name();
	  if (entry.is_locked())
	    cout << " L";
	  else
	    cout << "  ";

	  if (!left_column)
	    {
	      cout << "\n";
	    }
	  left_column = !left_column;
	}
      cout << "\n";
      return true;
    }
  };

  REGISTER_COMMAND(CommandCat);
}

namespace DFS
{

bool cmd_cat(const StorageConfiguration& storage, const DFSContext& ctx,
	     const vector<string>& args)
{
  auto instance = CIReg::get_command("cat");
  return (*instance)(storage, ctx, args);
}

}  // namespace DFS
