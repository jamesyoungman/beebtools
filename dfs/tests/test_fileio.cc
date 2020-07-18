//
//   Copyright 2020 James Youngman
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
#include <assert.h>      // for assert
#include <stdio.h>       // for perror, remove, fclose, fopen, fputc, EOF, FILE
#include <stdlib.h>      // for mkstemp, NULL
#include <unistd.h>      // for close
#include <algorithm>     // for all_of, max
#include <array>         // for array<>::const_iterator, array
#include <functional>    // for function
#include <iostream>      // for operator<<, basic_ostream::operator<<, ...
#include <optional>      // for optional
#include <string>        // for string, ...
#include <vector>        // for vector<>::const_iterator, vector<>::iterator

#include "abstractio.h"  // for SECTOR_BYTES, DataAccess, FileAccess, Sector...
#include "cleanup.h"     // for cleanup
#include "dfstypes.h"    // for byte, sector_count_type
#include "geometry.h"    // for Encoding, Geometry, Encoding::FM
#include "img_fileio.h"  // for FileView, OsFile
#include "img_sdf.h"     // for FilePresentedBlockwise

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

  bool block_is(DFS::FileAccess& acc, int sec, int val)
  {
    unsigned long pos = sec * DFS::SECTOR_BYTES;
    std::vector<DFS::byte> got = acc.read(pos, DFS::SECTOR_BYTES);
    if (got.size() != DFS::SECTOR_BYTES)
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
    if (!std::all_of(got.cbegin(), got.cend(), is_correct_val))
      {
	return false;
      }
    return true;
  }

  bool block_is(DFS::DataAccess& acc, int sec, int val)
  {
    std::optional<DFS::SectorBuffer> got = acc.read_block(sec);
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

    std::vector<DFS::byte> beyond_eof = f.read(test_blocks * DFS::SECTOR_BYTES, 1);
    if (!beyond_eof.empty())
      {
	std::cerr << "read beyond EOF succeeded\n";
	return false;
      }
    std::cerr << "PASS: test_osfile\n";
    return true;
  }


  bool test_fileview(const std::string& name, DFS::sector_count_type maxblocks)
  {
    DFS::internal::OsFile underlying(name);
    DFS::FilePresentedBlockwise block_io(underlying);
    const DFS::Geometry geom(3, 2, 2, DFS::Encoding::FM);
    assert(geom.total_sectors() <= maxblocks);
    DFS::internal::FileView v(block_io, name,
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
      test_fileview(file_name, TEST_FILE_BLOCKS);
  }
}


int main()
{
  return self_test() ? 0 : 1;
}
