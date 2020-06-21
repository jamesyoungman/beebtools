#include "commands.h"

#include <string.h>

#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

#include "crc.h"
#include "dfs.h"
#include "dfs_filesystem.h"
#include "media.h"
#include "storage.h"
#include "stringutil.h"

using std::cerr;
using std::string;
using DFS::stringutil::rtrim;

namespace
{
  bool create_inf_file(const string& name,
		       unsigned long crc,
		       const DFS::CatalogEntry& entry)
  {
    unsigned long load_addr = DFS::sign_extend(entry.load_address());
    unsigned long exec_addr = DFS::sign_extend(entry.exec_address());
    std::ofstream inf_file(name, std::ofstream::out);
    inf_file << std::hex << std::uppercase;
    // The NEXT field is missing because our source is not tape.
    using std::setw;
    using std::setfill;
    inf_file << entry.directory() << '.' << entry.name() << ' '
	     << setw(6) << setfill('0') << load_addr << " "
	     << setw(6) << setfill('0') << exec_addr << " "
	     << setw(6) << setfill('0') << entry.file_length() << " " // no sign-extend
	     << (entry.is_locked() ? "Locked " : "")
	     << "CRC=" << setw(4) << crc
	     << "\n";
    inf_file.close();
    return inf_file.good();
  }

class CommandExtractFiles : public DFS::CommandInterface
{
public:
  const std::string name() const override
  {
    return "extract-files";
  }

  const std::string usage() const override
  {
    return "usage: " + name() + " destination-directory\n"
      "All files from the selected drive (see the --drive global option) are\n"
      "extracted into the destination directory.\n"
      "\n"
      "If the DFS directory of the file is not the same as the current\n"
      "directory (selected with --dir) then the output file has a prefix\n"
      "D. where D is the file's DFS directory.  Either way, the DFS directory\n"
      "is given in the .inf file.  If you want the generated files to always\n"
      "contain a the DFS directory prefix, use --dir=. (this works because\n"
      ". is not a valid DFS directory name, so none of the DFS files will\n"
      "have that as their directory.\n"
      "\n"
      "The destination directory must exist already.  An archive .inf file is\n"
      "generated for each file.\n";
  }

  const std::string description() const override
  {
    return "extract all the files from the disc";
  }

  bool operator()(const DFS::StorageConfiguration& storage,
		  const DFS::DFSContext& ctx,
		  const std::vector<std::string>& args) override
  {
    // Use the --drive option to select which drive to extract files from.
    if (args.size() < 2)
      {
	cerr << "extract-all: please specify the destination directory.\n";
	return false;
      }
    if (args.size() > 2)
      {
	cerr << "extract-all: just one argument (the destination directory) is needed.\n";
	return false;
      }
    string dest_dir(args[1]);
    if (dest_dir.back() != '/')
      dest_dir.push_back('/');

    std::string error;
    auto mounted = storage.mount(ctx.current_drive, error);
    if (!mounted)
      {
	cerr << "failed to select drive " << ctx.current_drive << ": " << error << "\n";
	return false;
      }
    DFS::FileSystem* file_system = mounted->file_system();
    const DFS::Catalog& catalog(file_system->root());

    std::vector<DFS::byte> file_body;
    for (const auto& entry : catalog.entries())
      {
	file_body.clear();
	DFS::CRC crc;
	const string output_origname(string(1, entry.directory()) + "." + rtrim(entry.name()));
	string output_basename;
	if (entry.directory() == ctx.current_directory)
	  {
	    output_basename = rtrim(entry.name());
	  }
	else
	  {
	    output_basename = string(1, entry.directory()) + "." + rtrim(entry.name());
	  }
	const string output_body_file = dest_dir + output_basename;

	std::ofstream outfile(output_body_file, std::ofstream::out);

	auto ok = entry.visit_file_body_piecewise
	  (file_system->device(),
	   [&crc, &outfile, &output_body_file]
	   (const DFS::byte* begin,
	    const DFS::byte* end)
	   {
	     crc.update(begin, end);
	     outfile.write(reinterpret_cast<const char*>(begin),
			   end - begin);
	     if (!outfile)
	       {
		 std::cerr << output_body_file
			   << ": "
			   << strerror(errno) << "\n";
		 return false;
	       }
	     return true;
	   });
	outfile.close();
	if (!ok)
	  return false;
	const string inf_file_name = output_body_file + ".inf";
	if (!create_inf_file(inf_file_name, crc.get(), entry))
	  {
	    std::cerr << inf_file_name << ": " << strerror(errno) << "\n";
	    return false;
	  }
      }
    return true;
  }
};
REGISTER_COMMAND(CommandExtractFiles);

} // namespace
