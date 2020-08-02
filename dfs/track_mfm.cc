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

#include <functional>
#include <iomanip>
#include <string>
#include <sstream>

#include "crc.h"
#include "hexdump.h"

namespace
{
using Track::byte;

/* check_crc_with_a1s computes a CCIT CRC.
 *
 * We use scan_for() to locate sector headers and records.  It finds
 * the sequence of three A1 bytes which precede both of these in an
 * MFM track.  Those are followed by the address mark byte and the
 * data.  But the CRC is computed also over the A1 bytes.  Because
 * scan_for has already consumed those bits, we just add them into the
 * CRC calculation here.
 */
bool check_crc_with_a1s(const std::vector<byte>& data, std::string& error)
{
  static const byte a1bytes[] = {0xA1, 0xA1, 0xA1};
  DFS::CCITT_CRC16 crc;
  crc.update(a1bytes, a1bytes+sizeof(a1bytes));
  crc.update(data.data(), data.data() + data.size());
  if (crc.get())
    {
      std::ostringstream ss;
      ss << "CRC mismatch in block of " << std::dec << data.size()
	 << " bytes: 0x" << std::hex << crc.get() << " should be 0\n";
      error = ss.str();
      return false;
    }
  return true;
}

class MfmBitStream : public Track::BitStream
{
public:
  explicit MfmBitStream(const std::vector<byte>& data)
    : BitStream(data)
  {
  }

  std::optional<byte> read_byte(size_t& pos, std::string& error) const
  {
    // An FM-encoded byte occupies 16 bits on the disc, and looks like
    // this (in the order bits appear on disc):
    //
    // first       last
    // cDcDcDcDcDcDcDcD (c are clock bits, D data)
    assert(pos > 0u);
    auto began_at = pos;
    bool prev_data_bit = getbit(began_at - 1u);
    unsigned int data=0;
    for (int bitnum = 0; bitnum < 8; ++bitnum)
	{
	  if (pos + 2 >= size())
	    {
	      error = "unexpected end-of-track";
	      return std::nullopt;
	    }
	  const int clock_bit = getbit(pos++);
	  const int data_bit = getbit(pos++);
	  const int expected_clock = (prev_data_bit || data_bit) ? 0 : 1;
	  prev_data_bit = data_bit;

	  if (clock_bit != expected_clock)
	    {
	      std::ostringstream ss;
	      ss << "at track bit position " << pos
		 << " (" << (pos-began_at) << " bits into the data block)"
		 << ", MFM clock bit was "
		 << clock_bit << " where " << expected_clock << " was expected";
	      error = ss.str();
	      return std::nullopt;
	    }
	  data  = (data  << 1) | data_bit;
	}
    return data;
  }

  bool copy_mfm_bytes(size_t& thisbit, size_t n, std::vector<byte>* out,
		      std::string& error) const
  {
    while (n--)
	{
	  std::optional<byte> data = read_byte(thisbit, error);
	  if (data)
	    {
	      out->push_back(*data);
	      continue;
	    }
	  return false;
	}
    return true;
  }
};

}  // namespace

namespace Track
{
IbmMfmDecoder::IbmMfmDecoder(bool verbose)
  : verbose_(verbose)
{
}

std::vector<Sector> IbmMfmDecoder::decode(const std::vector<byte>& raw_data)
{
  self_test_crc();

  std::vector<Sector> result;
  const MfmBitStream bits(raw_data);
  size_t bits_avail = bits.size();
  size_t thisbit = 0;
  enum class MfmDecodeState { LookingForSectorHeader, LookingForRecord };
  Sector sec;
  int sec_size;
  enum MfmDecodeState state = MfmDecodeState::LookingForSectorHeader;
  while (bits_avail)
    {
      // Look for the bytes leading up to an address mark:
      // the last sync byte (data=0x00, clock=0xFF)
      // three bytes of (data=0xA1, clock=0x0A)
      auto found = bits.scan_for(thisbit,
				 0xAAAA448944894489,
				 0xFFFFFFFFFFFFFFFF);
      if (!found)
	break;
      thisbit = found->first + 1;
      // The next byte is an address mark; either the ID address mark
      // (which appears after gap3) or the data address mark (which
      // appears after gap2).
      //
      // We ignore gap1 (the post-index gap) since (a) it appears not
      // to exist in the formats we care about and (b) it makes no
      // difference the read case.
      switch (state)
	{
	case MfmDecodeState::LookingForSectorHeader:
	  {
	    // Contents of header (not including the three A1 bytes over
	    // which the CRC also gets computed).
	    // byte 0 - mark (0xFE)
	    // byte 1 - cylinder
	    // byte 2 - head (side)
	    // byte 3 - record (sector, starts from 0 in Acorn)
	    // byte 4 - size code (see switch below)
	    // byte 5 - CRC byte 1
	    // byte 6 - CRC byte 2
	    std::string error;
	    std::vector<byte> header;
	    if (bits.copy_mfm_bytes(thisbit, 7, &header, error))
	      {
		if (verbose_)
		  {
		    std::cerr << "read " << std::dec << header.size()
			      << " bytes of data:\n";
		    DFS::hexdump_bytes(std::cerr, 0, sizeof(header),
				       header.data(), header.data() + header.size());
		  }
		if (check_crc_with_a1s(header, error))
		  {
		    if (decode_sector_address_and_size(header.data(), &sec.address, &sec_size,
						       error))
		      {
			state = MfmDecodeState::LookingForRecord;
			continue;
		      }
		  }
	      }
	    if (verbose_)
	      {
		std::cerr << "Failed to read sector address: " << error << "\n";
	      }
	  }
	  state = MfmDecodeState::LookingForSectorHeader;
	  continue;

	case MfmDecodeState::LookingForRecord:
	  {
	    // The data over which the CRC is computed is the three A1 bytes plus:
	    // byte 0: marker byte (data_address_mark FB or deleted_data_address_mark F8)
	    // byte 1: initial byte of sector (which has size SEC_SIZE)
	    // byte 2 + sec_size: first byte of CRC
	    // byte 3 + sec_size: second byte of CRC
	    std::string error;
	    std::vector<byte> mark_and_data;
	    if (bits.copy_mfm_bytes(thisbit, sec_size + 3, // see above for extra bytes
				    &mark_and_data, error))
	      {
		if (verbose_)
		  {
		    std::cerr << "read " << std::dec << mark_and_data.size()
			      << " bytes of sector data:\n";
		    DFS::hexdump_bytes(std::cerr, 0, 16,
				       mark_and_data.data(),
				       mark_and_data.data() + mark_and_data.size());
		  }
		if (check_crc_with_a1s(mark_and_data, error))
		  {
		    const auto is_data = mark_and_data[0] == data_address_mark ? true : false;
		    if (is_data)
		      {
			sec.crc[0] = mark_and_data[sec_size + 1];
			sec.crc[1] = mark_and_data[sec_size + 2];
			sec.data.resize(sec_size);
			std::copy(mark_and_data.begin() + 1,
				  mark_and_data.begin() + 1 + sec_size,
				  sec.data.begin());
			if (verbose_)
			  {
			    std::cerr << "Accepting record/sector with address "
				      << sec.address << "; " << "it has "
				      << sec.data.size() << " bytes of data.\n";
			  }
			result.push_back(sec);
		      }
		    state = MfmDecodeState::LookingForSectorHeader;
		    continue;
		  }
		else
		  {
		    if (verbose_)
		      {
			std::cerr << "Failed to read sector " << sec.address
				  << ": " << error << "\n";
		      }
		    state = MfmDecodeState::LookingForSectorHeader;
		    continue;
		  }
	      }
	    if (verbose_)
	      std::cerr << "Failed to read sector data: " << error << "\n";
	  }
	  state = MfmDecodeState::LookingForSectorHeader;
	  continue;
	}
    }
  return result;
}

}  // namespace Track
