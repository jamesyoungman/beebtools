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
#ifndef INC_TRACK_H /* -*- Mode: C++ -*- */
#define INC_TRACK_H 1

#include <iosfwd>  // for ostream
#include <vector>  // for vector

struct SectorAddress
{
  unsigned char cylinder;
  unsigned char head;
  unsigned char record;		// note: 1-based

  bool operator<(const SectorAddress& a) const;
  bool operator==(const SectorAddress& a) const;
  bool operator!=(const SectorAddress& a) const;
};

namespace std
{
  ostream& operator<<(ostream&, const SectorAddress&);
}  // namespace std

struct Sector
{
  SectorAddress address;
  // We don't yield control sectors.
  std::vector<unsigned char> data;
  unsigned char crc[2];
};

// IbmFmDecoder decodes an FM track into sectors.
class IbmFmDecoder
{
public:
  IbmFmDecoder(bool verbose);

  // decode() decodes an FM data stream (as clock/data byte pairs)
  // into a sector. If no more sectors are available in source, nullopt
  // is returned.  source must be initialized such that the first byte
  // it returns is after the index mark and before the sync field.
  //
  // Only data sectors are returned.
  std::vector<Sector> decode(const std::vector<unsigned char>& track);

private:
  bool verbose_;
};

#endif
