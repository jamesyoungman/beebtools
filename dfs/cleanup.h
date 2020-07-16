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
#ifndef INC_CLEANUP_H
#define INC_CLEANUP_H 1

#include <functional>

class cleanup
{
 public:
  cleanup(std::function<void()> cleaner)
    : run_(cleaner) {}

  ~cleanup()
    {
      run_();
    }

 private:
  std::function<void()> run_;
};

// Saves and restores format flags on an ostream instance.
class ostream_flag_saver
{
public:
  ostream_flag_saver(std::ostream& os)
    : os_(os), saved_(os.flags())
  {
  }

  ~ostream_flag_saver()
  {
    os_.flags(saved_);
  }

private:
  std::ostream& os_;
  std::ostream::fmtflags saved_;
};

#endif
