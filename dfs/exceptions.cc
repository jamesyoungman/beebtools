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
#include "exceptions.h"

#include <string.h>  // for strerror
#include <sstream>   // for operator<<, ostringstream, basic_ostream, basic_...

namespace
{
  std::string make_errno_message(int errno_value)
  {
    const char* s = strerror(errno_value);
    if (s)
      return std::string(s);
    std::ostringstream ss;
    ss << "unknown errno value " << errno_value;
    return ss.str();
  }

  std::string make_file_errno_message(const std::string& file_name, int errno_value)
  {
    return file_name + ": " + make_errno_message(errno_value);
  }
}


namespace DFS {

std::string Unrecognized::make_msg(const std::string& cause)
{
  std::ostringstream ss;
  ss <<  "file format was not recognized: " << cause;
  return ss.str();
}


BadFileSystem::BadFileSystem(const std::string& msg)
  : BaseException(std::string("bad disk image: ") + msg)
{
}

DFS::BadFileSystem eof_in_catalog()
{
  return BadFileSystem("file system image is too short to contain a catalog");
}

FileIOError::FileIOError(const std::string& file_name, int errno_value)
  : BaseException(make_file_errno_message(file_name, errno_value))
{
}

NonFileOsError::NonFileOsError(int errno_value)
  : BaseException(make_errno_message(errno_value))
{
}

}  // namespace DFS
