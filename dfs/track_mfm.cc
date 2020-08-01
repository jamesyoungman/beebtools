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

bool check_crc(const byte* begin, const byte* end, std::string& error)
{
  DFS::CCITT_CRC16 crc;
  crc.update(begin, end);
  if (crc.get())
    {
      std::ostringstream ss;
      ss << "CRC mismatch in block of " << std::dec << (end - begin)
	 << " bytes: 0x" << std::hex << crc.get() << " should be 0\n";
      error = ss.str();
      return false;
    }
  return true;
}

}  // namespace

namespace Track
{
  class MfmBitStream : public BitStream
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
	  const int clock_bit = getbit(pos);
	  //std::cerr << "clock bit @" << pos << "=" << clock_bit << "\n";
	  ++pos;

	  const int data_bit = getbit(pos);
	  //std::cerr << " data bit @" << pos << "=" << data_bit << "\n";
	  ++pos;

	  const int expected_clock = (prev_data_bit || data_bit) ? 0 : 1;
	  //std::cerr << " check: expected clock " << expected_clock
	  //	    << ", got clock " << clock_bit << "\n";
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

    bool copy_mfm_bytes(size_t& thisbit, size_t n, byte* out, std::string& error) const
    {
      while (n--)
	{
	  auto data = read_byte(thisbit, error);
	  if (data)
	    {
	      *out++ = *data;
	      continue;
	    }
	  return false;
	}
      return true;
    }
  };


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

  auto hexdump_bits =
    [&bits](std::ostream& os, size_t pos, size_t len)
    {
      assert((len % 8u) == 0u);
      assert(pos + len < bits.size());
      std::vector<byte> data;
      data.resize(len / 8u);
      for (size_t n = 0; n < len; n += 8u)
	{
	  unsigned int b = 0u;
	  for (unsigned int i = 0; i < 8u; ++i)
	    {
	      b = (b << 1u) | (bits.getbit(pos + n + i) ? 1 : 0);
	    }
	  data.push_back(static_cast<byte>(b & 0xFFu));
	}
      size_t stride = 16;
      if (len/8 < stride)
	stride = len/8;
      DFS::hexdump_bytes(os, 0, stride, data.data(), data.data() + data.size());
    };

  enum class MfmDecodeState { LookingForSectorHeader, LookingForRecord };

  auto get_sector_address_and_size =
    [&bits, &thisbit](SectorAddress* address,
	      int *sec_size,
	      bool verbose,
	      std::string& error) -> bool
    {
      // Contents of header (over which the CRC gets computed).
      // byte 0 - fixed value A1
      // byte 1 - fixed value A1
      // byte 2 - fixed value A1
      // byte 3 - mark (0xFE)
      // byte 4 - cylinder
      // byte 5 - head (side)
      // byte 6 - record (sector, starts from 0 in Acorn)
      // byte 7 - size code (see switch below)
      // byte 8 - CRC byte 1
      // byte 9 - CRC byte 2
      byte header[10];
      // We used scan_for() to locate exactly the A1 A1 A1 byte pattern, so
      // we know it is actually there.  However, we already consumed it.
      header[0] = header[1] = header[2] = 0xA1;
      if (!bits.copy_mfm_bytes(thisbit, sizeof(header)-3u, &header[3], error))
	{
	  return false;
	}
      if (verbose)
	{
	  size_t len = sizeof(header)-3u;
	  std::cerr << "read " << std::dec << len << " bytes of header:\n";
	  DFS::hexdump_bytes(std::cerr, 0, len, &header[3], &header[3]+len);
	}
      if (!check_crc(header, header+sizeof(header), error))
	{
	  return false;
	}
      if (header[3] != 0xFE)
	{
	  // This is not a sector ID address mark (even though the
	  // CRC apparently matched).
	  std::ostringstream ss;
	  ss << "expected address mark byte 0xFE, found " << std::hex << std::uppercase
	     << std::setfill('0') << std::setw(2) << unsigned(header[3]);
	  error = ss.str();
	  return false;
	}
      if (verbose)
	{
	  std::cerr << "Sector ID address mark found:\n";
	  DFS::hexdump_bytes(std::cerr, 0, sizeof(header),
			     &header[0], &header[sizeof(header)]);
	}

      address->cylinder = header[4];
      address->head = header[5];
      address->record = header[6];
      std::optional<int> ssiz = decode_sector_size(header[7]);
      if (!ssiz)
	{
	  if (verbose)
	    {
	      std::cerr << "saw unexpected sector size code " << header[7] << "\n";
	    }
	  return false;
	}
      *sec_size = *ssiz;
      // bytes 8 and 9 are the CRC which we already checked.
      return true;
    };

  auto get_record_data =
    // XXX: fix the parameter order below to be more sensible.
    [&bits, &thisbit, this](int sec_size,
			     Sector& sec,
			     bool& is_data,
			     std::string& error) -> bool
    {
      // The data over which the CRC is computed is:
      // byte 0: A1
      // byte 1: A1
      // byte 2: A1
      // byte 3: marker byte (data_address_mark FB or deleted_data_address_mark F8)
      // byte 4: initial byte of sector (which has size SEC_SIZE)
      // byte 4 + sec_size: first byte of CRC
      // byte 5 + sec_size: second byte of CRC
      auto size_with_header_and_crc = sec_size + 6; // see above for extra bytes
      std::vector<byte> mark_and_data;
      mark_and_data.resize(size_with_header_and_crc);
      mark_and_data[0] = mark_and_data[1] = mark_and_data[2] = 0xA1;

      if (!bits.copy_mfm_bytes(thisbit,
			       mark_and_data.size()-3u,
			       mark_and_data.data()+3u, error))
	{
	  return false;
	}
      if (verbose_)
	{
	  const size_t len = mark_and_data.size()- 3u;
	  std::cerr << "read " << std::dec << len
		    << " bytes of sector data:\n";
	  DFS::hexdump_bytes(std::cerr, 0, 16,
			     mark_and_data.data() + 3u,
			     mark_and_data.data() + mark_and_data.size());
	}
      if (!check_crc(mark_and_data.data(),
		     mark_and_data.data() + mark_and_data.size(),
		     error))
	{
	  return false;
	}
      is_data = mark_and_data[3] == data_address_mark ? true : false;
      sec.crc[0] = mark_and_data[sec_size + 4];
      sec.crc[1] = mark_and_data[sec_size + 5];
      if (!is_data)
	return true;
      sec.data.resize(sec_size);
      std::copy(mark_and_data.begin() + 4,
		mark_and_data.begin() + 4 + sec_size,
		sec.data.begin());
      return true;
    };

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
      // The next byte is an address mark.
      std::string error;
      switch (state)
	{
	case MfmDecodeState::LookingForSectorHeader:
	  {
	    if (!get_sector_address_and_size(&sec.address, &sec_size, verbose_, error))
	      {
		if (verbose_)
		  {
		    std::cerr << "Failed to read sector address: " << error << "\n";
		  }
		state = MfmDecodeState::LookingForSectorHeader;
		continue;
	      }
	  }
	  state = MfmDecodeState::LookingForRecord;
	  continue;

	case MfmDecodeState::LookingForRecord:
	  {
	    bool is_data = false;
            const bool good_record = get_record_data(sec_size, sec, is_data, error);
            if (good_record)
	      {
		if (is_data)
		  {
		    if (verbose_)
		      {
			std::cerr << "Accepting record/sector with address "
				  << sec.address << "; " << "it has "
				  << sec.data.size() << " bytes of data.\n";
		      }
		    result.push_back(sec);
		  }
		else
		  {
		    if (verbose_)
		      std::cerr << "Dropping the control record " << sec.address << "\n";
		  }
	      }
	    else
	      {
		if (verbose_)
		  std::cerr << "Failed to read sector " << sec.address
			    << ": " << error << "\n";
	      }

	  }
	  state = MfmDecodeState::LookingForSectorHeader;
	  continue;
	}
    }
  return result;
}

}  // namespace Track
