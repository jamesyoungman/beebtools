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
#ifndef INC_EXCEPTIONS_H
#define INC_EXCEPTIONS_H

#include <exception>
#include <string>            // for string, allocator

namespace DFS {

class BaseException : public std::exception
{
public:
  explicit BaseException(const std::string& msg)
    : msg_(msg)
  {
  }

  const char* what() const noexcept
  {
    return msg_.c_str();
  }

private:
  std::string msg_;
};


class Unrecognized : public BaseException
{
public:
  explicit Unrecognized(const std::string& cause)
    : BaseException(make_msg(cause))
  {
  }

private:
  static std::string make_msg(const std::string& cause);
};

// OpusDDOS support is incomplete.  We throw this exception when
// encountering a case where the format makes a difference.
class OpusUnsupported : public BaseException
{
public:
  OpusUnsupported()
    : BaseException("Opus DDOS is not yet supported")
  {
  }
};

class FailedToGuessFormat : public BaseException
{
 public:
  explicit FailedToGuessFormat(const std::string& msg)
    : BaseException(msg)
  {
  }
};

class MediaNotPresent : public BaseException
{
public:
  explicit MediaNotPresent(const std::string& s)
    : BaseException(s)
  {
  }
};

class BadFileSystem : public BaseException
{
 public:
  BadFileSystem(const std::string& msg);
};

BadFileSystem eof_in_catalog();

class FileIOError : public BaseException
{
public:
  explicit FileIOError(const std::string& file_name, int errno_value);
};

class NonFileOsError : public BaseException
{
public:
  // NonFileOsError is only for operations which don't involve a
  // file. For operations involving a file, use FileIOError instead.
  explicit NonFileOsError(int errno_value);
};

}  // namespace DFS
#endif
