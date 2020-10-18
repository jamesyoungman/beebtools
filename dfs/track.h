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
#ifndef INC_TRACK_H
#define INC_TRACK_H 1

#include <iosfwd>		// for ostream
#include <iostream>		// for cerr
#include <optional>		// for optional
#include <vector>		// for vector
#include <sstream>		// for std::ostringstream

#include "dfstypes.h"

namespace Track
{
using byte = unsigned char;

constexpr int normal_fm_clock = 0xFF;
constexpr int id_address_mark = 0xFE;
constexpr int data_address_mark = 0xFB;
constexpr int deleted_data_address_mark = 0xF8;

void self_test_crc();

struct SectorAddress
{
  unsigned char cylinder;
  unsigned char head;
  unsigned char record;		// note: 1-based

  bool operator<(const SectorAddress& a) const;
  bool operator==(const SectorAddress& a) const;
  bool operator!=(const SectorAddress& a) const;
};

}  // namsepace Track

namespace std
{
  ostream& operator<<(ostream&, const Track::SectorAddress&);
}  // namespace std

namespace Track
{
struct Sector
{
  SectorAddress address;
  // We don't yield control sectors.
  std::vector<unsigned char> data;
  unsigned char crc[2];

  bool operator<(const Sector& other) const
  {
    return address < other.address;
  }
};

class BitStream
{
public:
  explicit BitStream(const std::vector<byte>& data, size_t first_bit, size_t stride)
    : input_(data), raw_bit_size_(data.size() * 8), first_(first_bit), stride_(stride)
  {
  }

  size_t raw_pos(size_t bitpos) const
  {
    return bitpos * stride_ + first_;
  }

  bool getbit(size_t bitpos) const
  {
    return rawbit(raw_pos(bitpos));
  }

  bool rawbit(size_t raw_bitpos) const
  {
    const size_t i = raw_bitpos / 8;
    const size_t b = raw_bitpos % 8;
    return input_[i] & (1 << b);
  }

  std::optional<std::pair<size_t, int64_t>> scan_for(size_t start,
						     uint64_t val,
						     uint64_t mask) const
  {
    const uint64_t needle = mask & val;
    uint64_t shifter = 0, got = 0;
    size_t i_cooked = start;
    for (size_t i = raw_pos(start); i < raw_bit_size_; ++i_cooked, i += stride_)
      {
	shifter = (shifter << 1u) | (rawbit(i) ? 1u : 0u);
	got = (got << 1u) | 1u;
	if ((mask & got) == mask)
	  {
	    // We have enough bits for the comparison to be valid.
	    if ((mask & shifter) == needle)
	      return std::make_pair(i_cooked, shifter);
	  }
      }
    return std::nullopt;
  }

  size_t size() const
  {
    return (raw_bit_size_ - first_) / stride_;
  }

private:
  const std::vector<byte>& input_;
  const size_t raw_bit_size_;
  const size_t first_;
  const size_t stride_;
};

// decode_fm_track() decodes an FM data stream (as clock/data bit pairs)
// into a sector. If no more sectors are available in source, nullopt
// is returned.  source must be initialized such that the first byte
// it returns is after the index mark and before the sync field.
//
// Only data sectors are returned.
std::vector<Sector> decode_fm_track(const BitStream& track, bool verbose);

// decode_mfm_track() decodes an MFM data stream (as clock/data bit
// pairs) into a sector. source must be initialized such that the
// first byte it returns is after the index mark and before the sync
// field.
//
// Only data sectors are returned.
std::vector<Sector> decode_mfm_track(const BitStream& track, bool verbose);

/* reverse the ordering of bits in a byte. */
inline byte reverse_bit_order(Track::byte in)
{
  int out = 0;
  if (in & 0x80)  out |= 0x01;
  if (in & 0x40)  out |= 0x02;
  if (in & 0x20)  out |= 0x04;
  if (in & 0x10)  out |= 0x08;
  if (in & 0x08)  out |= 0x10;
  if (in & 0x04)  out |= 0x20;
  if (in & 0x02)  out |= 0x40;
  if (in & 0x01)  out |= 0x80;
  return static_cast<byte>(out);
}


bool decode_sector_address_and_size(const byte* header, SectorAddress* sec, int* siz,
				    std::string& error);
}  // namespace Track

namespace DFS
{
bool check_track_is_supported(const std::vector<Track::Sector> track,
			      unsigned int track_number,
			      unsigned int side,
			      unsigned int sector_bytes,
			      bool verbose,
			      std::string& error);

}  // namespace DFS

#endif
