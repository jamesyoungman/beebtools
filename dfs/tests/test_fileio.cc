#include <unistd.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

#include "img_fileio.h"
#include "cleanup.h"

namespace
{
  bool prepare_test_file(const std::string& name, int maxblocks)
  {
    bool good = false;
    FILE *fp = fopen(name.c_str(), "wb");
    if (NULL == fp)
      {
	perror(name.c_str());
	return false;
      }
    cleanup delete_on_fail([&good, name]()
			   {
			     if (!good)
			       remove(name.c_str());
			   });

    for (int i = 0; i < maxblocks; ++i)
      {
	const char ch = static_cast<char>(i);
	for (unsigned j = 0; j < DFS::SECTOR_BYTES; ++j)
	  {
	    if (EOF == fputc(ch, fp))
	      {
		perror(name.c_str());
		return false;
	      }
	  }
      }
    if (EOF == fclose(fp))
      {
	perror(name.c_str());
      }
    else
      {
	good = true;
      }
    return good;

  }

  bool block_is(DFS::DataAccess& acc, int sec, int val)
  {
    auto got = acc.read_block(sec);
    if (!got)
      {
	std::cerr << "failed to read block " << sec << "\n";
	return false;
      }
    auto is_correct_val = [sec, val](DFS::byte b) -> bool {
			    if (b == val) return true;
			    std::cerr << "wrong data in block " << sec
				      << "; expected " << int(val)
				      << ", got " << int(b) << "\n";
			    return false;
			  };
    if (!std::all_of(got->cbegin(), got->cend(), is_correct_val))
      {
	return false;
      }
    return true;
  }

  bool test_osfile(const std::string& file_name, int test_blocks)
  {
    DFS::internal::OsFile f(file_name);
    for (int i = 0; i < test_blocks; ++i)
      {
	if (!block_is(f, i, i))
	  return false;
      }
    auto can_read_beyond_eof = f.read_block(test_blocks);
    if (can_read_beyond_eof)
      {
	std::cerr << "read beyond EOF succeeded\n";
	return false;
      }
    std::cerr << "PASS: test_osfile\n";
    return true;
  }

  bool test_narrowedfileview(const std::string& file_name, int test_blocks)
  {
    DFS::internal::OsFile underlying(file_name);
    DFS::internal::NarrowedFileView same(underlying, 0, DFS::sector_count(test_blocks));
    for (int i = 0; i < test_blocks; ++i)
      {
	if (!block_is(same, i, i))
	  return false;
      }

    DFS::internal::NarrowedFileView short_view(underlying, 0, DFS::sector_count(2));
    for (int i = 0; i < 2; ++i)
      {
	if (!block_is(short_view, i, i))
	  return false;
      }
    if (short_view.read_block(2))
      {
	std::cerr << "read beyond EOF in short_view NarrowedFileView\n";
	return false;
      }

    DFS::internal::NarrowedFileView middle(underlying, 3, DFS::sector_count(2));
    for (int i = 0; i < 2; ++i)
      {
	if (!block_is(middle, i, i+3))
	  return false;
      }
    if (middle.read_block(2))
      {
	std::cerr << "read beyond EOF in middle NarrowedFileView\n";
	return false;
      }

    std::cerr << "PASS: test_narrowedfileview\n";
    return true;
  }

  bool test_fileview(const std::string& name, DFS::sector_count_type maxblocks)
  {
    DFS::internal::OsFile underlying(name);
    const DFS::Geometry geom(3, 2, 2, DFS::Encoding::FM);
    assert(geom.total_sectors() <= maxblocks);
    DFS::internal::FileView v(underlying, name,
			      "test file", geom,
			      1, // initial skip
			      2, // take
			      3, // leave
			      geom.total_sectors());
    // skip 1: check we skipped
    if (!block_is(v, 0, 1))
      return false;
    // take 2: check the values
    if (!block_is(v, 1, 2))
      return false;
    // leave 3 (blocks 3, 4, 5), so the next sectors must contain (taking 2) 6, 7.
    if (!block_is(v, 2, 6))
      return false;
    if (!block_is(v, 3, 7))
      return false;
    // leave 3 again
    if (!block_is(v, 4, 11))
      return false;
    if (v.read_block(12))
      {
	std::cerr << "read beyond EOF in test_fileview\n";
	return false;
      }
    std::cerr << "PASS: test_fileview\n";
    return true;
  }


  bool self_test()
  {
    const std::string name_tmpl = "test_osfile_XXXXXXXXXX";
    std::vector<char> tmp_file_name(name_tmpl.begin(), name_tmpl.end());
    tmp_file_name.push_back(0);
    const int fd = mkstemp(tmp_file_name.data());
    cleanup close_and_delete([&tmp_file_name, fd]()
			     {
			       close(fd);
			       remove(tmp_file_name.data());
			     });
    constexpr int TEST_FILE_BLOCKS = 12;
    if (!prepare_test_file(tmp_file_name.data(), TEST_FILE_BLOCKS))
      return false;
    const std::string file_name(tmp_file_name.begin(), tmp_file_name.end());

    return
      test_osfile(file_name, TEST_FILE_BLOCKS) &&
      test_narrowedfileview(file_name, TEST_FILE_BLOCKS) &&
      test_fileview(file_name, TEST_FILE_BLOCKS);
  }
}


int main()
{
  return self_test() ? 0 : 1;
}
