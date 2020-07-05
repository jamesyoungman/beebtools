#include "commands.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

#include "cleanup.h"
#include "commands.h"
#include "dfs.h"
#include "dfs_volume.h"
#include "driveselector.h"
#include "stringutil.h"

using std::string;
using std::vector;

namespace
{
  using DFS::Catalog;
  using DFS::CatalogEntry;
  using DFS::FileSystem;
  using DFS::Format;

  class colstream
  {
  public:
    colstream(std::ostream& os, const std::string& lineprefix)
      : col_(0u), forward_to_(os), line_prefix_(lineprefix)
    {
      emit_prefix();
    }

    void emit_prefix()
    {
      // This isn't supposed to cause col_ to change.
      for (char ch : line_prefix_)
	forward_to_.put(ch);
    }

    void set_prefix(const std::string& s)
    {
      line_prefix_ = s;
    }

    string::size_type current_column() const
    {
      return col_;
    }

    colstream& advance_to_column(std::string::size_type n)
    {
      if (col_ > n)
	put('\n');
      while (col_ < n)
	put(' ');
      return *this;
    }

    template <class T>
    colstream& operator<<(const T& item)
    {
      std::ostringstream ss;
      ss.copyfmt(forward_to_);
      ss << item;
      // Implementation limitation: if the format of item contains \n,
      // we will not emit the prefix after it, which is probably a
      // bug.
      forward_to_ << item;
      update_col(ss.str());
      return *this;
    }

    colstream& put(char ch)
    {
      forward_to_.put(ch);
      update_col(ch);
      if (ch == '\n')
	emit_prefix();
      return *this;
    }

    colstream& write(const char* s, std::streamsize n)
    {
      forward_to_.write(s, n);
      update_col(s, n);
      return *this;
    }

  colstream& operator<< (std::ostream& (*f)(std::ostream &)) {
    f(forward_to_);
    return *this;
  }

    colstream& operator<< (std::ostream& (*f)(std::ios &)) {
      f(forward_to_);
      return *this;
    }

    colstream& operator<< (std::ostream& (*f)(std::ios_base &)) {
      f(forward_to_);
      return *this;
    }

  private:
    static constexpr int TAB_WIDTH = 8;

    void update_col(char ch)
    {
      if (ch == '\n' || ch == '\r')
	{
	  col_ = 0;
	}
      else if (ch == '\t')
	{
	  tab();
	}
      else
	{
	  ++col_;
	}
    }

    void update_col(const std::string& s)
    {
      for (char ch : s)
	update_col(ch);
    }

    void update_col(const char* s, std::streamsize n)
    {
      while (n--)
	update_col(*s++);
    }

    void tab()
    {
      auto oldcol = col_;
      auto past = col_ % TAB_WIDTH;
      if (!past)
	{
	  col_ += TAB_WIDTH;
	}
      else
	{
	  col_ += (TAB_WIDTH - past);
	}
      assert(col_ > oldcol);
      assert((col_ % TAB_WIDTH) == 0);
    }

    std::string::size_type col_;
    std::ostream& forward_to_;
    std::string line_prefix_;
  };

  std::string title_and_cycle(DFS::UiStyle ui,
			      const std::string& title,
			      std::optional<int> cycle)
  {
    const int title_width = ui == DFS::UiStyle::Opus ? 0 : 12;
    std::ostringstream os;
    os << std::left << std::setw(title_width) << std::setfill(' ')
       << title;
    if (cycle)
      {
	if (ui != DFS::UiStyle::Opus || !title.empty())
	  {
	    os << ' ';
	  }
	os << std::setbase(16);
	os << '(' << std::setfill('0') << std::right << std::setw(2)
	   << (*cycle) << ')';
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
      case DFS::UiStyle::Opus:
	return double_density ? "Double density" : "Single density";
      default:
	return double_density ? "MFM" : "FM";
      }
  }

  bool boot_setting_in_upper_case(DFS::UiStyle ui)
  {
    switch (ui)
      {
      case DFS::UiStyle::Opus:
      case DFS::UiStyle::Watford:
	return false;
      case DFS::UiStyle::Acorn:
	// also case DFS::UiStyle::HDFS:
      case DFS::UiStyle::Default:
      default:
	return true;
      }
  }

  std::string describe_boot_setting(const DFS::BootSetting& opt, DFS::UiStyle ui)
  {
    std::ostringstream ss;
    std::string desc, display;
    desc = description(opt);
    display.resize(desc.size());
    std::function<int(int)> uc_lc = boot_setting_in_upper_case(ui) ?
      [](int ch) -> int { return std::toupper(static_cast<unsigned int>(ch)); } :
      [](int ch) -> int { return std::tolower(static_cast<unsigned int>(ch)); };
    std::transform(desc.begin(), desc.end(), display.begin(), uc_lc);
    ss << std::dec << value(opt) << " (" << display << ")";
    return ss.str();
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

    static std::optional<int> get_screen_cols()
    {
      // $COLUMNS (if it is set) is the width of the user's terminal.
      // Hence it applies only if the output is actually going to a
      // terminal.
      if (0 == isatty(STDOUT_FILENO))
	{
	  // Either stdout is not a terminal or isatty failed.  in
	  // either case the caller will have to use a default.
	  return std::nullopt;
	}
      const char * cols = std::getenv("COLUMNS");
      if (cols)
	{
	  char *end;
	  const long n = std::strtol(cols, &end, 10);
	  if (n)
	    {
	      if (n >= std::numeric_limits<int>::min() || n <= std::numeric_limits<int>::max())
		{
		  return static_cast<int>(n);
		}
	      else
		{
		  // Unrepresentable, we will choose a sane default.
		  return std::nullopt;  // COLUMNS=999999999999999999999999999
		}
	    }
	  else
	    {
	      return std::nullopt;  // COLUMNS=0 or COLUMNS=not-a-number
	    }
	}
      // In theory we could do something complex here, such as
      // initialising curses and asking it to probe the terminal size,
      // but that's a lot of complexity. Instead we just use a
      // default.
      return std::nullopt;  // $COLUMNS is not set
    }

    // Some DFS implementations produce an adaptive number of columns:
    //
    // DFS variant      Mode 2        Mode 7       Mode 0
    //                  [20 cols]     [40 cols]    [80 cols]
    // Acorn            2 (w=1)       2            2
    // Watford          4 (w=1)       4 (w=2)      4
    // HDFS             1             2            4
    // Solidisk         2 (w=1)       2            2
    // Opus             2 (w=1)       2            2
    //
    // Taking Watford DFS as an example, it always produces 4
    // columns of output.  However, since the screen width is always
    // a multiple of 20, in modes 7 and 2 this appears to be
    // 2-column and 1-column output, respectively (which is what w=2
    // and w=1 means in the table above).  Similarly, Acorn DFS
    // always produces 2 colums of output, but this appears as 1
    // column in mode 2.
    //
    // We are producing output for systems whose terminal width is not
    // always a multiple of 20, and so we cannot take the same
    // approach.  The output_columns() function returns the actual
    // number of columns we should produce, which follows the w=N
    // values where these are different (so that we generate the same
    // appearance).
    static int select_output_columns(DFS::UiStyle ui, int screen_width)
    {
      switch (ui)
	{
	case DFS::UiStyle::Watford:
	  if (screen_width < 40)
	    return 1;
	  else if (screen_width < 80)
	    return 2;
	  else
	    return 4;

	case DFS::UiStyle::Default:
	case DFS::UiStyle::Acorn:
	case DFS::UiStyle::Opus:
	default:
	  return screen_width < 40 ? 1 : 2;

	}
    }

    bool invoke(const DFS::StorageConfiguration& storage,
		const DFS::DFSContext& ctx,
		const std::vector<std::string>& args) override
    {
      const std::optional<int> screen_width = get_screen_cols();
      if (DFS::verbose)
	{
	  std::cerr << "Screen width is ";
	  if (screen_width)
	    std::cerr << *screen_width;
	  else
	    std::cerr << "unknown or inapplicable";
	  std::cerr << "\n";
	}
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
	  d = ctx.current_volume;
	}
      auto mounted = storage.mount(d, error);
      if (!mounted)
	return faild(d);
      DFS::FileSystem* file_system = mounted->file_system();
      const Catalog& catalog(mounted->volume()->root());
      DFS::Geometry geom = file_system->geometry();
      const auto ui = file_system->ui_style(ctx);

      std::cout << std::setfill(' ');
      colstream out(std::cout, ui == DFS::UiStyle::Opus ? " " : "");

      // Produce output which is suitable for the actual width of the device
      // and the selected ui.
      constexpr int col_width = 20;
      const std::string::size_type rmargin =
	select_output_columns(ui, screen_width.value_or(40)) * col_width;

      std::string::size_type current_col = 0;
      auto next_line = [&current_col](colstream& cs)
		       {
			 cs.put('\n');
			 current_col = 0;
		       };
      auto next_column = [&current_col, rmargin, col_width](colstream& cs)
			 {
			   ++current_col;
			   auto nextpos = current_col * col_width;
			   if (cs.current_column() == nextpos)
			     nextpos += col_width;

			   if (nextpos >= rmargin)
			     {
			       cs.put('\n');
			       current_col = 0;
			     }
			   else
			     {
			       cs.advance_to_column(nextpos);
			     }
			 };

      out << title_and_cycle(ui, catalog.title(), catalog.sequence_number());

      // In Watford DFS, the density is shown in the following column.
      // In Opus DDOS, the density is shown on the following line.
      // In Acorn DFS, it's just printed after a space.
      switch (ui)
	{
	case DFS::UiStyle::Watford:
	  next_column(out);
	  break;
	case DFS::UiStyle::Opus:
	  next_line(out);
	  break;
	default:
	  out << " ";
	  break;
	}
      out << density_desc(geom, ui) << std::setbase(10);

      if (ui == DFS::UiStyle::Opus)
	{
	  const std::string labels("ABCDEFGH");
	  std::string subvol_summary;
	  subvol_summary.reserve(8);
	  std::vector<std::optional<char>> subvolumes = file_system->subvolumes();
	  bool has_subvolumes = false;
	  for (char ch : labels)
	    {
	      if (std::find(subvolumes.begin(), subvolumes.end(), ch) == subvolumes.end())
		ch = '.';
	      else
		has_subvolumes = true;
	      subvol_summary.push_back(ch);
	    }
	  if (has_subvolumes)
	    {
	      next_column(out);
	      out << subvol_summary;
	    }
	}
      next_line(out);

      out << "Drive "<< d;
      next_column(out);
      out << "Option " << describe_boot_setting(catalog.boot_setting(), ui);
      next_line(out);

      std::string dir_label, lib_label;
      switch (ui)
	{
	case DFS::UiStyle::Acorn:
	  // Actually the Acorn 8271 DFS ROM uses the unabbreviated
	  // words too.  Only the Acorn 1770 DFS ROM uses the
	  // abbreviated form, but we don't distinguish those variants
	  // in the UI.
	  // TODO: HDFS uses the abbreviated forms too.
	  dir_label = "Dir.";
	  lib_label = "Lib.";
	  break;
	default:
	  dir_label = "Directory";
	  lib_label = "Library";
	  break;
	}

      out << dir_label << " :" << ctx.current_volume << "." << ctx.current_directory;
      next_column(out);
      out << lib_label << " :0.$";

      if (DFS::UiStyle::Watford == ui)
	{
	  next_column(out);
	  out << "Work file $.";
	}
      // In Opus DDOS, only the header itself has a leading space.
      out.set_prefix("");
      next_line(out);
      next_line(out);

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

      bool printed_gap = false;
      out << std::left;

      bool first = true;
      for (const auto& entry : entries)
	{
	  if (entry.directory() != ctx.current_directory)
	    {
	      if (!printed_gap)
		{
		  if (out.current_column() > 0)
		    next_line(out);
		  next_line(out);
		  printed_gap = true;
		  first = true;
		}
	    }

	  if (first)
	    first = false;
	  else
	    next_column(out);

	  std::ostringstream os;
	  os << std::setw(DFS::UiStyle::Watford == ui ? 3 : 2) << "";
	  if (entry.directory() != ctx.current_directory)
	    os << entry.directory() << ".";
	  else
	    os << "  ";
	  os << entry.name();
	  out << std::setw(8) << std::right << os.str();
	  if (entry.is_locked())
	    {
	      out << std::setfill(' ') << std::setw(5) << std::right << "L";
	    }
	}
      next_line(out);
      if (DFS::UiStyle::Opus == ui)
	{
	  if (entries.empty())
	    out << "No file\n";
	}
      else if (DFS::UiStyle::Watford == ui)
	{
	  next_line(out);
	  out << std::setw(2) << std::setfill('0') << std::right << entries.size()
	      << " files of " << catalog.max_file_count()
	      << " on " << geom.cylinders << " tracks\n"
	      << std::setfill(' ');
	}
      return true;
    }
  };

  REGISTER_COMMAND(CommandCat);
}
