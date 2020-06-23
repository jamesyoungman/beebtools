#include "commands.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "commands.h"
#include "dfs_volume.h"
#include "driveselector.h"
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

  std::string title_and_cycle(const std::string& title,
			      std::optional<int> cycle)
  {
    std::ostringstream os;
    int title_width = 0;
    os << std::left << std::setw(title_width) << std::setfill(' ')
       << title;
    if (cycle)
      {
	os << std::setbase(16);
	os << " (" << std::setfill('0') << std::right << std::setw(2)
	   << (*cycle) << ")";
      }
    return os.str();
  }

  std::string density_desc(std::optional<DFS::Geometry> geom,
			   DFS::UiStyle ui)
  {
    const bool double_density = geom && (geom->encoding == DFS::Encoding::MFM);
    switch (ui)
      {
      case DFS::UiStyle::Watford:
	return double_density ? "Double density" : "Single density";
      default:
	return double_density ? "MFM" : "FM";
      }
  }

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

    bool invoke(const DFS::StorageConfiguration& storage,
		const DFS::DFSContext& ctx,
		const std::vector<std::string>& args) override
    {
      std::string error;
      auto fail = [&error]()
		  {
		    std::cerr << error << "\n";
		    return false;
		  };
      auto faild = [&error](DFS::VolumeSelector vol)
		  {
		    std::cerr << "failed to select drive " << vol << ": " << error << "\n";
		    return false;
		  };
      DFS::VolumeSelector d(0);
      if (args.size() > 2)
	{
	  std::cerr << "Please specify at most one argument, the drive number\n";
	  return false;
	}
      else if (args.size() == 2)
	{
	  error.clear();
	  size_t end;
	  auto got = DFS::VolumeSelector::parse(args[1], &end, error);
	  if (!got)
	    return fail();
	  if (end != args[1].size())
	    {
	      std::cerr << "unexpected suffix on drive specifier " << args[1] << "\n";
	      return false;
	    }
	  d = *got;
	  if (!error.empty())
	    std::cerr << "warning: " << error << "\n";
	}
      else
	{
	  d = ctx.current_drive;
	}
      auto mounted = storage.mount(d, error);
      if (!mounted)
	return faild(d);
      DFS::FileSystem* file_system = mounted->file_system();
      const Catalog& catalog(mounted->volume()->root());
      const auto ui = file_system->ui_style(ctx);

      DFS::Geometry geom = file_system->geometry();
      cout << std::setw(DFS::UiStyle::Watford == ui ? 20 : 0)
          << std::setfill(' ')
          << std::left
          << title_and_cycle(catalog.title(), catalog.sequence_number());
      cout << (DFS::UiStyle::Watford == ui ? "" : " ")
	   << density_desc(geom, ui) << std::setbase(10) << "\n";

      const int left_col_width = 20;
      cout << "Drive "<< std::setw(left_col_width-6) << d
	   << "Option " << catalog.boot_setting() << "\n";

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
      cout << "\n";
      if (DFS::UiStyle::Watford == ui)
	{
	  cout << std::setw(2) << std::setfill('0') << std::right << entries.size()
	       << " files of " << catalog.max_file_count()
	       << " on " << geom.cylinders << " tracks\n";
	}
      return true;
    }
  };

  REGISTER_COMMAND(CommandCat);
}
