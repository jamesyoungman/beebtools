#include <getopt.h>
#include <limits.h>
#include <string.h>

#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "commands.h"
#include "dfs.h"
#include "driveselector.h"
#include "media.h"
#include "storage.h"

namespace
{
  using DFS::StorageConfiguration;

  std::optional<DFS::UiStyle> parse_ui_style(const std::string& name, std::string& err)
  {
    std::map<std::string, DFS::UiStyle> styles =
      {
       {"Acorn",   DFS::UiStyle::Acorn},
       {"acorn",   DFS::UiStyle::Acorn},
       {"watford", DFS::UiStyle::Watford},
       {"Watford", DFS::UiStyle::Watford},
       {"Opus",    DFS::UiStyle::Opus},
       {"opus",    DFS::UiStyle::Opus},
      };
    auto it = styles.find(name);
    if (it != styles.end())
      return it->second;
    std::ostringstream os;
    os << "Unknown UI style " << name << "; known UI styles are ";
    bool comma = false;
    for (const auto& mapping : styles)
      {
	// Don't print the initial-uppercase names, we won't use those
	// as the canonical ones (while they are proper nouns,
	// lower-case fits better with the CLI conventions).
	if (mapping.first.empty() || isupper(mapping.first[0]))
	  continue;
	if (comma)
	  os << ", ";
	else
	  comma = true;
	os << mapping.first;
      }
    err = os.str();
    return std::nullopt;
  }

  enum OptSignifier
    {
     OPT_IMAGE_FILE = SCHAR_MIN,
     OPT_CWD,
     OPT_DRIVE,
     OPT_SHOW_CONFIG,
     OPT_ALLOCATE_PHYSICAL,
     OPT_ALLOCATE_FIRST,
     OPT_UI_STYLE,
     OPT_VERBOSE,
     OPT_HELP,
    };

  // struct option fields: name, has_arg, *flag, val
  const struct option global_opts[] =
    {
     // --file controls which disc image file we open
     { "file", 1, NULL, OPT_IMAGE_FILE },
     // --dir controls which directory the program should believe is
     // current (as for *DIR).
     { "dir", 1, NULL, OPT_CWD },
     // --drive controls which drive the program should believe is
     // associated with the disc image specified in --file (as for
     // *DRIVE).
     { "drive", 1, NULL, OPT_DRIVE },
     // --drive-first allows disc images to be "inserted" into the first free slot
     { "drive-first", 0, NULL, OPT_ALLOCATE_FIRST },
     // --drive-physical allows disc images to be "inserted" as if
     // they were physical disks (e.g. for single-sided files into 0
     // then 1, then 4).
     { "drive-physical", 0, NULL, OPT_ALLOCATE_PHYSICAL },
     { "show-config", 0, NULL, OPT_SHOW_CONFIG },
     { "help", 0, NULL, OPT_HELP },
     { "ui", 1, NULL, OPT_UI_STYLE },
     { "verbose", 0, NULL, OPT_VERBOSE },
     { 0, 0, 0, 0 },
    };

  bool check_consistency()
  {
    const auto& option_help = DFS::get_option_help();
    bool ok = true;
    std::set<std::string> global_opt_names;
    // Check that all global options have a help string.
    for (const auto& opt : global_opts)
      {
	if (opt.name == 0)
	  break;
	global_opt_names.insert(opt.name);
	auto it = option_help.find(opt.name);
	if (it == option_help.end())
	  {
	    std::cerr << "option_help lacks entry for --" << opt.name << "\n";
	    ok = false;
	  }
      }
    // Check that all option help strings match a global option.
    for (const auto& h : option_help)
      {
	if (global_opt_names.find(h.first) == global_opt_names.end())
	  {
	    std::cerr << "help has entry for "
		      << h.first << " but that's not an actual option "
		      << "in global_opts.\n";
	    ok = false;
	  }
      }
    return ok;
  }

  std::pair<bool, DFS::VolumeSelector> get_drive_number(const char *s)
  {
    DFS::VolumeSelector v(0);
    bool ok = [s, &v]() {
		size_t end;
		std::string specifier(s);
		std::string err;
		std::optional<DFS::VolumeSelector> got = DFS::VolumeSelector::parse(specifier, &end, err);
		if (!got)
		  {
		    std::cerr << err << "\n";
		    return false;
		  }
		if (end < specifier.size())
		  {
		    std::cerr << "Unexpected suffix '" << specifier.substr(end)
			      << "' in argument '" << specifier << "' to --drive\n";
		    return false;
		  }
		v = *got;
		return true;
	      }();
    return std::make_pair(ok, v);
  }

std::unique_ptr<std::map<std::string, std::string>> option_help;

std::unique_ptr<std::map<std::string, std::string>> make_option_help()
{
  const std::map<std::string, std::string>
    m({
       {"file", "the name of the DFS image file to read"},
       {"dir", "the default directory (if unspecified, use $)"},
       {"drive", "the default drive (if unspecified, use 0)"},
       {"drive-first", "disc images are assigned the next free drive slot"},
       {"drive-physical", "disc images are assigned drive slots as if they were physical discs "
	"(as if they were physical floppies being inserted)"},
       {"show-config", "show the storage configuration before "
	"performing the operation"},
       {"ui", "follow the user-interface of this type of DFS ROM"},
       {"help", "print a brief explanation of how to use the program"},
       {"verbose", "print (on stderr) messages about the operation of the program"}
      });
  return std::make_unique<std::map<std::string, std::string>>(m);
}

}  // namespace

namespace DFS
{
  const std::map<std::string, std::string>& get_option_help()
  {
    if (!option_help)
      {
	option_help = make_option_help();
      }
    return *(option_help.get());
  }
}  // namespace DFS

int main (int argc, char *argv[])
{
  if (!check_consistency())
    return 2;
  DFS::DFSContext ctx('$', DFS::VolumeSelector(0));
  int longindex;
  std::vector<std::string> extra_args;
  // files is just a way to manage the lifetime of the image file objects such
  // that they live longer than the StorageConfiguration.
  std::vector<std::unique_ptr<DFS::AbstractImageFile>> files;
  DFS::StorageConfiguration storage; // must be declared after files.
  bool show_config = false;
  DFS::DriveAllocation how_to_allocate_drives(DFS::DriveAllocation::PHYSICAL);
  int opt;
  while ((opt=getopt_long(argc, argv, "+", global_opts, &longindex)) != -1)
    {
      switch (opt)
	{
	case '?':  // Unknown option or missing option argument.
	  // An error message was already issued.
	  return 1;

	case OPT_IMAGE_FILE:
	  try
	    {
	      std::string error;
	      std::unique_ptr<DFS::AbstractImageFile> file = DFS::make_image_file(optarg, error);
	      if (!file)
		{
		  std::cerr << error << '\n';
		  return 1;
		}
	      if (!file->connect_drives(&storage, how_to_allocate_drives, error))
		{
		  std::cerr << error << '\n';
		  return 1;
		}
	      files.push_back(std::move(file));
	    }
	  catch (std::exception& e)
	    {
	      std::cerr << "cannot use image file " << optarg << ": "
		   << e.what() << "\n";
	      return 1;
	    }
	  break;

	case OPT_CWD:
	  if (strlen(optarg) != 1)
	    {
	      std::cerr << "Argument to --" << global_opts[longindex].name
			<< " should have one character only.\n";
	      return 1;
	    }
	  ctx.current_directory = optarg[0];
	  break;

	case OPT_DRIVE:
	  {
	    bool ok;
	    std::tie(ok, ctx.current_volume) = get_drive_number(optarg);
	    if (!ok)
	      return 1;
	  }
	  break;

	case OPT_ALLOCATE_FIRST:
	  how_to_allocate_drives = DFS::DriveAllocation::FIRST;
	  break;

	case OPT_ALLOCATE_PHYSICAL:
	  how_to_allocate_drives = DFS::DriveAllocation::PHYSICAL;
	  break;

	case OPT_SHOW_CONFIG:
	  show_config = true;
	  break;

	case OPT_UI_STYLE:
	  {
	    std::string err;
	    auto ui_opt = parse_ui_style(optarg, err);
	    if (!ui_opt)
	      {
		std::cerr << err << "\n";
		return 1;
	      }
	    ctx = DFS::DFSContext(ctx.current_directory, ctx.current_volume,
				  *ui_opt);
	    break;
	  }

	case OPT_VERBOSE:
	  DFS::verbose = true;
	  break;

	case OPT_HELP:
	  {
	    DFS::CommandHelp help;
	    return help.invoke(storage, ctx, extra_args) ? 0 : 1;
	  }
	}
    }
  if (optind == argc)
    {
      std::cerr << "Please specify a command (try \"help\")\n";
      return 1;
    }
  const std::string cmd_name = argv[optind];
  if (optind < argc)
    extra_args.assign(&argv[optind], &argv[argc]);

  auto instance = DFS::CIReg::get_command(cmd_name);
  if (0 == instance)
    {
      std::cerr << "unknown command " << cmd_name << "\n";
      return 1;
    }

  try
    {
      if (show_config)
	{
	  storage.show_drive_configuration(std::cerr);
	}
      return instance->invoke(storage, ctx, extra_args) ? 0 : 1;
    }
  catch (std::exception& e)
    {
      std::cerr << "error: " << e.what() << "\n";
      return 1;
    }
}
