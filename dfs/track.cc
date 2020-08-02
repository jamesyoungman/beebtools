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
#include "track.h"

#include <algorithm>	        // for is_sorted
#include <functional>	        // for function<>
#include <stdint.h>	        // for uint8_t
#include <stddef.h>             // for size_t
#include <cassert>              // for assert
#include <iomanip>              // for operator<<, setw
#include <iostream>             // for operator<<, basic_ostream, ostream
#include <optional>             // for optional
#include <string>               // for operator<<
#include <utility>              // for make_pair, pair

#include "crc.h"                // for CCITT_CRC16
#include "hexdump.h"

#undef ULTRA_VERBOSE

using Track::Sector;
using Track::SectorAddress;

namespace Track
{


void self_test_crc()
{
  DFS::CCITT_CRC16 crc;
  uint8_t input[2];
  input[0] = uint8_t('T');
  input[1] = uint8_t('Q');

  crc.update(input, input+1);
  assert(crc.get() == 0xFB81);
  crc.update(input+1, input+2);
  assert(crc.get() == 0x95A0);

  DFS::CCITT_CRC16 crc2;
  crc2.update(input, input+2);
  assert(crc.get() == 0x95A0);


  const uint8_t id[7] = {
			 Track::id_address_mark,
			 0x4F,	// cylinder
			 0,	// side
			 6,	// sector
			 1,	// size code
			 0xE1,	// CRC byte 1
			 0x07   // CRC byte 1
  };
  DFS::CCITT_CRC16 crc3;
  crc3.update(id, id+sizeof(id));
  assert(crc3.get() == 0);
}

bool decode_sector_address_and_size(const byte* header, SectorAddress* address,
				    int* siz, std::string& error)
{
  if (header[0] != 0xFE)
    {
      std::ostringstream ss;
      ss << "expected address mark byte 0xFE, found " << std::hex << std::uppercase
	 << std::setfill('0') << std::setw(2) << unsigned(header[0]);
      error = ss.str();
      return false;
    }
  address->cylinder = header[1];
  address->head = header[2];
  address->record = header[3];

  switch (header[4])
    {
    case 0x00:
      *siz = 128;
      break;
    case 0x01:
      *siz = 256;
      break;
    case 0x02:
      *siz = 512;
      break;
    case 0x03:
      *siz = 1024;
      break;
    default:
      {
	std::ostringstream ss;
	ss << "saw unexpected sector size code " << header[4] << "\n";
	error = ss.str();
	return false;
      }
    }
  return true;
}

bool SectorAddress::operator<(const SectorAddress& a) const
{
  if (cylinder < a.cylinder)
    return true;
  if (cylinder > a.cylinder)
    return false;
  if (head < a.head)
    return true;
  if (head > a.head)
    return false;
  if (record < a.record)
    return true;
  if (record > a.record)
    return false;
  return false;			// equal.
}

bool SectorAddress::operator==(const SectorAddress& a) const
{
 if (*this < a)
   return false;
 if (a < *this)
   return false;
 return true;
}

bool SectorAddress::operator!=(const SectorAddress& a) const
{
 if (*this == a)
   return false;
 return true;
}

}  // namespace Track

namespace std
{
  ostream& operator<<(ostream& os, const SectorAddress& addr)
  {
    ostream::sentry s(os);
    if (s)
      {
	ostringstream ss;
	ss << '('
	   << unsigned(addr.cylinder) << ','
	   << unsigned(addr.head) << ','
	   << unsigned(addr.record)
	   << ')';
	os << ss.str();
      }
    return os;
  }

}  // namespace std

namespace DFS
{
  bool check_track_is_supported(const std::vector<Sector> track_sectors,
				unsigned int track,
				unsigned int side,
				unsigned int sector_bytes,
				bool verbose,
				std::string& error)
  {
    assert(std::is_sorted(track_sectors.begin(), track_sectors.end()));

    // Validate the sectors themselves.
    std::optional<int> prev_rec_num;
    for (const Sector& sect : track_sectors)
      {
	// Many of the possible issues detected here are more likely
	// to be a bug in our code than something weird about the
	// HFE file.
	std::ostringstream ss;
	if (sect.address.head != side)
	  {
	    ss << "found sector with address " << sect.address
	       << " in the data for side " << side;
	    error = ss.str();
	    return false;
	  }
	if (sect.address.cylinder != track)
	  {
	    ss << "found sector with address " << sect.address
	       << " in the data for track " << track;
	    error = ss.str();
	    return false;
	  }
	if (prev_rec_num)
	  {
	    if (*prev_rec_num == sect.address.record)
	      {
		ss << "sector with address " << sect.address
		   << " has a duplicate record number ";
		error = ss.str();
		return false;
	      }
	    else if (*prev_rec_num + 1 < sect.address.record)
	      {
		ss << "before sector with address " << sect.address
		   << " there is no sector with record number "
		   << (*prev_rec_num + 1);
		error = ss.str();
		return false;
	      }
	  }
	else
	  {
	    // This is the first record (numerically, not in
	    // physical order).  The IBM format specification
	    // numbers records from 1, but Acorn DFS uses 0 as the
	    // lowest sector number.
	    if (sect.address.record != 0)
	      {
		if (verbose)
		  {
		    std::cerr << "warning: the lowest-numbered sector of "
			      << "track " << track << " has address "
			      << sect.address
			      << " but it should have record number 0 "
			      << "instead of "
			      << unsigned(sect.address.record) << "\n";
		  }
	      }
	  }

	if (sect.data.size() != sector_bytes)
	  {
	    ss << "track " << track
	       << " contains a sector with address " << sect.address
	       << " but it has unsupported size " << sect.data.size()
	       << " (the supported size is " << sector_bytes << ")";
	    error = ss.str();
	    return false;
	  }

	prev_rec_num = sect.address.record;
      }
    return true;
  }

}  // namespace DFS
