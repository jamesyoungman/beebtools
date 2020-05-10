#include "commands.h"

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "afsp.h"
#include "dfs.h"

using std::cerr;
using std::cout;


class CommandInfo : public DFS::CommandInterface // *INFO
{
public:
  const std::string name() const override
  {
    return "info";
  }

  const std::string usage() const override
  {
    return "usage: " + name() + " wildcard\n"
      "The wildcard specifies which files information should be shown for.\n"
      "To specify all files, use the wildcard #.*\n"
      "Numeric values are shown in hexadecimal (base 16).\n"
      "\n"
      "The output fields are:\n"
      "  file name\n"
      "  'L' if the file is locked, otherwise spaces\n"
      "  load address (in hex)\n"
      "  execution address (in hex)\n"
      "  file length (in hex)\n"
      "  sector within the disc at which the file is stored (in hex)\n"
      "\n"
      "Load and execution addresses are sign-extended from their actual\n"
      "18 bit length (as stored in the disc catalogue) to 24 bits.\n"
      "For example, 3F1900 becomes FF1900.\n"
      "We do this for consistency with the Acorn DFS implementation.\n"
      // Reference for the significance of the 0x20000 bit is the
      // Watford DFS manual (section 9.1 "18 BIT ADDRESSING") and the
      // Master Reference Guide.
      "When the top bits (i.e. hex 20000) are set, this signifies that the\n"
      "file was saved from the I/O processor rather than the tube\n"
      "co-processor, and should be loaded back into the same processor.\n";
  }

  const std::string description() const override
  {
    return "display information about a file (for example load address)";
  }

  bool operator()(const DFS::StorageConfiguration& storage,
		  const DFS::DFSContext& ctx,
		  const std::vector<std::string>& args) override
  {
    if (args.size() < 2)
      {
	cerr << "info: please give a file name or wildcard specifying which files "
	     << "you want to see information about.\n";
	return false;
      }
    if (args.size() > 2)
      {
	cerr << "info: please specify no more than one argument (you specified "
	     << (args.size() - 1) << ")\n";
	return false;
      }
    const DFS::FileSystemImage *image;
    if (!storage.select_drive_by_afsp(args[1], &image, ctx.current_drive))
      return false;
    assert(image != 0);

    std::string error_message;
    std::unique_ptr<DFS::AFSPMatcher> matcher =
      DFS::AFSPMatcher::make_unique(ctx, args[1], &error_message);
    if (!matcher)
      {
	cerr << "Not a valid pattern (" << error_message << "): " << args[1] << "\n";
	return false;
      }

    const int entries = image->catalog_entry_count();
    cout << std::hex;
    cout << std::uppercase;
    using std::setw;
    using std::setfill;
    for (int i = 1; i <= entries; ++i)
      {
	const auto& entry = image->get_catalog_entry(i);
	const std::string full_name = std::string(1, entry.directory()) + "." + entry.name();
#if VERBOSE_FOR_TESTS
	std::cerr << "info: directory is '" << entry.directory() << "'\n";
	std::cerr << "info: item is '" << full_name << "'\n";
#endif
	if (!matcher->matches(full_name))
	  continue;
	unsigned long load_addr, exec_addr;
	load_addr = DFS::sign_extend(entry.load_address());
	exec_addr = DFS::sign_extend(entry.exec_address());
	cout << entry.directory() << "." << entry.name() << " "
	     << (entry.is_locked() ? " L " : "   ")
	     << setw(6) << setfill('0') << load_addr << " "
	     << setw(6) << setfill('0') << exec_addr << " "
	     << setw(6) << setfill('0') << entry.file_length() << " "
	     << setw(3) << setfill('0') << entry.start_sector()
	     << "\n";
      }
    return true;
  }
};
REGISTER_COMMAND(CommandInfo);
