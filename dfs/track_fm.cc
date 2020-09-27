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

#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#include "crc.h"
#include "hexdump.h"

namespace
{
  unsigned long get_crc(const std::vector<Track::byte>& data)
  {
    DFS::CCITT_CRC16 crc;
    crc.update(data.data(), data.data() + data.size());
    return crc.get();
  }

}  // namespace

namespace Track
{

  class FmBitStream : public BitStream
  {
  public:
    explicit FmBitStream(const std::vector<byte>& data)
      : BitStream(data)
    {
    }

    std::optional<std::pair<byte, byte>> read_byte(size_t& start) const
    {
      // An FM-encoded byte occupies 16 bits on the disc, and looks like
      // this (in the order bits appear on disc):
      //
      // first       last
      // cDcDcDcDcDcDcDcD (c are clock bits, D data)
      unsigned int clock=0, data=0;
      for (int bitnum = 0; bitnum < 8; ++bitnum)
	{
	  if (start + 2 >= size())
	    return std::nullopt;

	  clock = (clock << 1) | getbit(start++);
	  data  = (data  << 1) | getbit(start++);
	}
      return std::make_pair(static_cast<unsigned char>(clock),
			    static_cast<unsigned char>(data));
    }

    bool copy_fm_bytes(size_t& thisbit, size_t n, std::vector<byte>* out, bool verbose) const
    {
      while (n--)
	{
	  auto clock_and_data = read_byte(thisbit);
	  if (clock_and_data && clock_and_data->first == normal_fm_clock)
	    {
	      out->push_back(clock_and_data->second);
	      continue;
	    }
	  if (verbose)
	    {
	      if (!clock_and_data)
		{
		  std::cerr << "end-of-track while reading data bytes\n";
		}
	      else
		{
		  std::cerr << "desynced while reading data bytes\n";
		}
	    }
	  return false;
	}
      return true;
    }
  };

IbmFmDecoder::IbmFmDecoder(bool verbose)
  : verbose_(verbose)
{
}


// Decode a train of FM clock/data bits into a sequence of zero or more
// sectors.
std::vector<Sector> IbmFmDecoder::decode(const std::vector<byte>& raw_data)
{
  self_test_crc();

  std::vector<Sector> result;
  // The initial value of shifter has no particular significance
  // except for the fact that we won't mistake it for part of the sync
  // sequence (which is all just clock bits, all the data bits are
  // zero) or an address mark.
  const FmBitStream bits(raw_data);
  size_t bits_avail = bits.size();
  size_t thisbit = 0;

  auto find_record_address_mark =
    [&thisbit, &bits, bits_avail]() -> std::optional<unsigned int>
    {
     while (thisbit < bits_avail)
       {
	/* We're searching for two bytes (though usually there are
	   more) of FM-encoded 0x00 followed by one of these:

	   0xF56A: control record
	   0xF56F: data record
	   But, (0xF56A & 0xF56F) == 0xF56A, so we scan for that and check what we got.
	   0xA == binary 1010

	   A data byte of 0 is 0xAAAA in clocked form (as the clock
	   bits are always 1 except for address marks and in gaps
	   where the data is indeterminate).  So 0xAAAAAAAA matches
	   two data bytes of 0x00 with normal FM clocks.
	*/
	auto found = bits.scan_for(thisbit,
				   0xAAAAAAAAF56A,
				   0xFFFFFFFFFFFA);
	if (!found)
	  {
	    thisbit = bits_avail;
	    break;
	  }
	found->second &= 0xFFFF;
	thisbit = found->first + 1;
	if (found->second == 0xF56A || found->second == 0xF56F)
	  {
	    return found->second;
	  }
	/* We could have seen some third bit pattern, for example
	   0xF56B.  But that pattern isn't a prefix of the pattern
	   we're searching for (because of the AAAA prefix) so it's
	   safe to just continue the search from the spot where we
	   found this bit pattern.
	*/
       }
     return std::nullopt;
    };

  enum class DecodeState { LookingForAddress, LookingForRecord };
  Sector sec;
  int sec_size;
  enum DecodeState state = DecodeState::LookingForAddress;
  while (thisbit < bits_avail)
    {
      if (state == DecodeState::LookingForAddress)
	{
	  // The FM-encoded sequence bit sequence 1111010101111110 has
	  // the hex value 0xF57E.  Dividing it into 4 nibbles we can
	  // visualise the clock and data bits:
	  //
	  // Hex  binary  clock  data
	  // F    1111     11..   11..
	  // 5    0101     ..00   ..11
	  //
	  // that is, for the top nibbles we have clock 1100=0xC and
	  // data 1111=0xF.
	  //
	  // Hex  binary  clock  data
	  // 7    0111     01..   11..
	  // E    1110     ..11   ..10
	  //
	  // that is, for the bottom nibbles we have clock 0111=0x7
	  // and data 1110=0xE.
	  //
	  // Putting together the nibbles we have data 0xC7, clock
	  // 0xFE.  That is address mark 1, which introduces the
	  // sector ID.  Address marks are unusual in that the clock
	  // bits are not all set to 1.
	  //
	  // The address mark is preceded by at least two FM-encoded
	  // zero bytes; 0x00 encodes to 0xAAAA.
	  auto found = bits.scan_for(thisbit,
				     0xAAAAAAAAF57E,
				     0xFFFFFFFFFFFF);
	  if (!found)
	    break;
	  thisbit = found->first + 1u;
	  if (verbose_)
	    {
#if ULTRA_VERBOSE
	      std::cerr << "Found AM1, reading sector address\n";
#endif
	    }
	  /* clock=0xC7, data=0xFE - this is the index address mark */
	  // Contents of the address
	  // byte 0 - mark (data, 0xFE)
	  // byte 1 - cylinder
	  // byte 2 - head (side)
	  // byte 3 - record (sector, starts from 0 in Acorn)
	  // byte 4 - size code (see switch below)
	  // byte 5 - CRC byte 1
	  // byte 6 - CRC byte 2
	  std::vector<byte> id;
	  id.push_back(byte(id_address_mark));
	  if (!bits.copy_fm_bytes(thisbit, 6u, &id, verbose_))
	    {
	      if (verbose_)
		{
		  std::cerr << "Failed to read sector address\n";
		}
	      state = DecodeState::LookingForAddress;
	      continue;
	    }
	  const auto addr_crc = get_crc(id);
	  if (addr_crc)
	    {
	      if (verbose_)
		{
		  std::cerr << "Sector address CRC mismatch: 0x"
			    << std::hex << addr_crc << " should be 0\n";
		}
	      state = DecodeState::LookingForAddress;
	      continue;
	    }

	  std::string error;
	  if (!decode_sector_address_and_size(id.data(), &sec.address, &sec_size, error))
	    {
	      if (verbose_)
		std::cerr << error << "\n";
	      state = DecodeState::LookingForAddress;
	      continue;
	    }
	  // id[5] and id[6] are the CRC bytes, and these already got
	  // included in our evaluation of addr_crc.
	  state = DecodeState::LookingForRecord;
	}
      else if (state == DecodeState::LookingForRecord)
	{
	  std::optional<unsigned int> found = find_record_address_mark();
	  if (!found)
	    break;
	  const bool discard_record = *found == 0xF56A;
	  if (verbose_)
	    {
	      std::cerr << "This record has address " << sec.address
			<< " and should contain "
			<< std::dec << sec_size << " bytes.  It is a "
			<< (discard_record ? "control" : "data")
			<< " record so we will "
			<< (discard_record ? "discard" : "keep")
			<< " it.\n";
	    }
	  // Read the sector itself.
	  size_t size_with_crc = sec_size + 2;  // add two bytes for the CRC
	  sec.data.resize(size_with_crc);
	  byte data_mark[1] =
	    {
	     byte(discard_record ? deleted_data_address_mark : data_address_mark)
	    };
	  sec.data.clear();
	  if (!bits.copy_fm_bytes(thisbit, size_with_crc, &sec.data, verbose_))
	    {
	      if (verbose_)
		{
		  std::cerr << "Lost sync in sector data\n";
		}
	      state = DecodeState::LookingForAddress;
	      continue;
	    }
	  DFS::CCITT_CRC16 crc;
	  crc.update(data_mark, data_mark+1);
	  crc.update(sec.data.data(), sec.data.data() + size_with_crc);
	  // If we already know the record is a control record
	  // (deleted / faulty) then we might expect the CRC to be
	  // incorrect (for example, because this part of the disc
	  // doesn't provide reliable reads).
	  auto data_crc = crc.get();
	  if (data_crc != 0 && !discard_record)
	    {
	      if (verbose_)
		{
		  std::cerr << "Sector data CRC mismatch: 0x"
			    << std::hex << data_crc << " should be 0; "
			    << "dropping the sector\n";
		  DFS::hexdump_bytes(std::cerr, 0, 1, data_mark, data_mark+1);
		  DFS::hexdump_bytes(std::cerr, 1, 32, sec.data.data(), sec.data.data() + size_with_crc);
		}
	      state = DecodeState::LookingForAddress;
	      continue;
	    }
	  sec.crc[0] = sec.data[sec_size + 0];
	  sec.crc[1] = sec.data[sec_size + 1];
	  // Resize the sector data downward to drop the CRC.
	  sec.data.resize(sec_size);

	  if (!discard_record)
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
	      std::cerr << "Dropping the control record\n";
	    }
	  state = DecodeState::LookingForAddress;
	}
    }
  return result;
}

}  // namespace Track
