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

bool read_and_check_crc(size_t nbytes,
			std::function<bool(size_t, byte *, std::string&)> reader,
			byte* out,
			std::string& error)
{
  if (!reader(nbytes, out, error))
    return false;
  DFS::CCITT_CRC16 crc;
  crc.update(out, out + nbytes);
  if (crc.get())
    {
      std::ostringstream ss;
      ss << "CRC mismatch in block of " << std::dec << nbytes
	 << " bytes: 0x" << std::hex << crc.get() << " should be 0\n";
      error = ss.str();
      return false;
    }
  return true;
}

std::pair<unsigned char, unsigned char> declock(unsigned int word)
{
  int clock = 0, data = 0;
  int bitnum = 0;
  while (bitnum < 16)
    {
      int clockbit = word & 0x8000 ? 1 : 0;
      clock = (clock << 1) | clockbit;
      word  = (word << 1) & 0xFFFF;
      ++bitnum;
      int databit = word & 0x8000 ? 1 : 0;
      data = (data << 1) | databit;
      word = (word << 1) & 0xFFFF;

      ++bitnum;
    }
  return std::make_pair(static_cast<unsigned char>(clock),
			static_cast<unsigned char>(data));
}

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
  unsigned int shifter = 0x0000;
  const BitStream bits(raw_data);
  size_t bits_avail = bits.size();
  size_t thisbit = 0;

  int bits_at_once = 1;
  enum MfmDecodeState { LookingForAddressA1, LookingForRecord };
  enum MfmDecodeState state = LookingForAddressA1;

  auto getbit = [&bits_avail, &thisbit, &bits]()
		{
		  const int result = bits.getbit(thisbit) ? 1 : 0;
		  --bits_avail;
		  ++thisbit;
		  return result;
		};
  // copy_bytes must only be called when the next bit to be read (that
  // is, the one at position i) is the clock bit which begins a new
  // byte.
  auto copy_bytes = [&bits_avail, &thisbit, &bits, this](size_t nbytes,
							 byte* out) -> bool
		    {
		      if (verbose_)
			{
#if ULTRA_VERBOSE
			  std::cerr << std::dec
				    << "copy_bytes: " << bits_avail
				    << " bits still left in track data; "
				    << (nbytes * 16)
				    << " bits needed to consume " << nbytes*2 << " clocked bytes "
				    << "and yield " << nbytes
				    << " bytes of actual data\n";
#endif
			}

		      if (bits_avail / 16 < nbytes)
			{
			  if (verbose_)
			    {
			      std::cerr << "copy_bytes: not enough track data left to copy "
					<< nbytes << " bytes\n";
			    }
			  return false;
			}
		      while (nbytes--)
			{
			  int clock=0, data=0, allbits=0;
			  for (int bitnum = 0; bitnum < 8; ++bitnum)
			    {
			      bool clock_bit = bits.getbit(thisbit++);
			      bool data_bit = bits.getbit(thisbit++);
			      clock = (clock << 1) | (clock_bit ? 1 : 0);
			      data  = (data  << 1) | (data_bit ? 1 : 0);
			      allbits = (allbits << 2) | ((clock_bit ? 2 : 0) | (data_bit ? 1 : 0));
			      bits_avail -= 2;
			    }
#if ULTRA_VERBOSE
			  if (verbose_)
			    {
			      std::cerr << std::hex
					<< "copy_bytes: from bits " << std::setw(4)
					<< unsigned(allbits)
					<< ", got clock="
					<< std::setw(2) << unsigned(clock)
					<< " (expected "
					<< std::setw(2) << unsigned(clock_expected)
					<< "), data=" << std::setw(2)
					<< unsigned(data)
					<< "\n";
			    }
#endif
#if 0
			  if (clock != clock_expected)
			    {
			      std::cerr << std::hex
					<< "copy_bytes: became desynchronised; got clock="
					<< std::setw(2) << unsigned(clock)
					<< ", data=" << std::setw(2)
					<< unsigned(data)
					<< " at track input byte " << (thisbit / 8u)
					<< "\n";
			      return false;
			    }
#endif
			  *out++ = static_cast<byte>(data);
			}
		      return true;
		    };

  auto reader = [copy_bytes, this](size_t len, byte *out, std::string& error)
		{
		  if (!copy_bytes(len, out))
		    {
		      error = "lost clock synchrionization";
		      return false;
		    }
		  if (verbose_)
		    {
		      std::cerr << "read " << std::dec << len << " bytes of data: ";
		      DFS::hexdump_bytes(std::cerr, len, 16, out, out+len);
		    }
		  return true;
		};

  constexpr int offset_field_width = 4;

  Sector sec;
  int sec_size;
  while (bits_avail)
    {
      for (int i = 0; i < 1; ++i)
	{
	  if (!bits_avail)
	    break;
	  const int newbit = getbit();
	  shifter = ((shifter << 1) | newbit) & 0xFFFF;

	  if (verbose_)
	    {
	      auto prevbit = thisbit-1;
	      auto n = prevbit/8;
	      auto mask = 1 << (prevbit % 8);
	      auto bit_val = (raw_data[n] & mask) ? 1 : 0;

	      if (n % 4 == 0 && mask == 1)
		{
		  auto bytes_remaining = bits_avail / 8; // underestimate, since truncation
		  DFS::hexdump_bytes(std::cerr, n, 16,
				     raw_data.data() + n,
				     raw_data.data() + n + std::min(16uL, bytes_remaining));

		}
	      std::cerr << "MFM track: " << std::hex << std::setfill('0')
			<< "newbit=" << newbit
			<< " [" << bit_val << "]"
			<< " from bit " << prevbit << " (= bit " << (prevbit % 8)
			<< " of byte at offset " << std::dec
			<< std::setw(offset_field_width)
			<< std::setfill(' ') << n
			<< ", having value " << std::hex << std::setw(2)
			<< unsigned(raw_data[n]) << ")\n";
	    }
	}
      if (!bits_avail)
	break;

      if (verbose_)
	{
	  auto [clock, data] = declock(shifter);
	  std::cerr << "MFM track: " << std::hex << std::setfill('0')
		    << "bits_at_once=" << bits_at_once
		    << ", shifter=" << std::setw(4) << shifter
		    << ", clock=" << std::setw(2) << unsigned(clock)
		    << ", data=" << std::setw(2) << unsigned(data)
		    << ", state="
		    << (state == LookingForAddressA1 ? "LookingForAddressA1" :
			(state == LookingForRecord ? "LookingForRecord" :
			 "unknown"))
		    << "\n";
	}
      if (shifter == 0xAAAA)  // clock 0xFF, data 0x00.
	{
	  if (verbose_)
	    {
	      std::cerr << "(We could switch to larger reads now)\n";
	    }
	  continue;
	}

      if (shifter != 0x4489)
	continue;

      if (verbose_)
	{
	  std::cerr << "We're now synchronised to the byte boundary.\n";
	}

      const std::string gap_name = (state == LookingForAddressA1) ?
	"gap3 (before address)" : "gap2 (before record)";
      byte gap_tail[2];
      if (!copy_bytes(2, gap_tail))
	{
	  std::cerr << "Desynced while reading the end of " << gap_name << "\n";
	  state = LookingForAddressA1;
	  continue;
	}
      if (verbose_)
	{
	  std::cerr << "We have found the end of " << gap_name << "\n";
	}

      switch (state)
	{
	case LookingForAddressA1:
	  {
	    // Contents of header
	    // byte 0 - mark (data, 0xFE)
	    // byte 1 - cylinder
	    // byte 2 - head (side)
	    // byte 3 - record (sector, starts from 0 in Acorn)
	    // byte 4 - size code (see switch below)
	    // byte 5 - CRC byte 1
	    // byte 6 - CRC byte 2
	    byte header[7];
	    std::string error;
	    if (!read_and_check_crc(7, reader, header, error))
	      {
		if (verbose_)
		  {
		    std::cerr << error << "\n";
		  }
		continue;
	      }
	    if (header[0] == 0xFE)
	      {
		if (verbose_)
		  {
		    std::cerr << "Sector ID address mark found:\n";
		    DFS::hexdump_bytes(std::cerr, 0, sizeof(header), &header[0], &header[sizeof(header)]);
		  }
	      }
	    else
	      {
		// This is not a sector ID but still, somehow, the CRC matched.
		if (verbose_)
		  {
		    std::cerr << "Address mark is " << std::hex << unsigned(header[0])
			      << " so this is not a sector ID address mark\n";
		    continue;
		  }
	      }
	    std::optional<int> ssiz = decode_sector_size(header[4]);
	    if (!ssiz)
	      {
		if (verbose_)
		  {
		    std::cerr << "saw unexpected sector size code " << header[4] << "\n";
		  }
		state = LookingForAddressA1;
		continue;
	      }
	    sec_size = *ssiz;
	    state = LookingForRecord;
	  }
	  continue;

	case LookingForRecord:
	  {
	    auto size_with_crc = sec_size + 3;  // add two bytes for the CRC, 1 for address mark
	    std::vector<byte> mark_and_data;
	    mark_and_data.resize(size_with_crc);
	    std::string error;
	    if (!read_and_check_crc(size_with_crc, reader, mark_and_data.data(), error))
	      {
		if (verbose_)
		  std::cerr << error << "\n";
		state = LookingForAddressA1;
		continue;
	      }
	    const bool discard_record = mark_and_data[0] == data_address_mark ? false : true;
	    sec.crc[0] = mark_and_data[sec_size + 1];
	    sec.crc[1] = mark_and_data[sec_size + 2];
	    if (discard_record)
	      {
		if (verbose_)
		  {
		    std::cerr << "Dropping the control record\n";
		  }
		state = LookingForAddressA1;
		continue;
	      }
	    if (verbose_)
	      {
		std::cerr << "Accepting record/sector with address "
			  << sec.address << "; " << "it has "
			  << sec.data.size() << " bytes of data.\n";
	      }
	    sec.data.resize(sec_size);
	    std::copy(mark_and_data.begin() + 1,
		      mark_and_data.begin() + 1 + sec_size,
		      sec.data.begin());
	    result.push_back(sec);
	    state = LookingForAddressA1;
	    continue;
	  }
	}
    }
  return result;
}

}  // namespace Track
