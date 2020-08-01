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
#include "crc.h"

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

    bool copy_fm_bytes(size_t& thisbit, size_t n, byte* out, bool verbose) const
    {
      while (n--)
	{
	  auto clock_and_data = read_byte(thisbit);
	  if (clock_and_data && clock_and_data->first == normal_fm_clock)
	    {
	      *out++ = clock_and_data->second;
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
    [&thisbit, &bits, bits_avail]() -> std::optional<std::pair<size_t, unsigned int>>
    {
     while (thisbit < bits_avail)
       {
	/* We're searching for one of these:
	   0xF56A: control record
	   0xF56F: data record
	   But, (0xF56A & 0xF56F) == 0xF56A, so we scan for that and check what we got.
	   0xA == binary 1010
	*/
	auto searched_from = thisbit;
	auto found = bits.scan_for(thisbit, 0xF56A, 0xFFFA);
	if (!found)
	  {
	    thisbit = bits_avail;
	    break;
	  }
	found->second &= 0xFFFF;
	if (found->second == 0xF56A || found->second == 0xF56F)
	  {
	    return std::make_optional(std::make_pair(found->first+1, found->second));
	  }
	/* We could have seen some third bit pattern, for
	   example 0xF56B.  A match exactly at the next bit
	   is impossible, but advancing by just one bit
	   keeps the code simpler and this case will be very
	   rare.
	*/
	thisbit = searched_from + 1;
       }
     return std::nullopt;
    };

  enum DecodeState { Desynced, LookingForAddress, LookingForRecord };
  enum DecodeState state = Desynced;
  Sector sec;
  int sec_size;
  while (thisbit < bits_avail)
    {
      if (state == Desynced)
	{
	  // The FM-encoded bit sequence 1010101010101010 (hex 0xAAAA)
	  // has all clock bits set to 1 and all data bits set to 0.
	  // It represents clock=0xFF, data=0x00.  This is the sync
	  // field.
	  auto found = bits.scan_for(thisbit, 0xAAAA, 0xFFFF);
	  if (!found)
	    break;
	  state = LookingForAddress;
	  thisbit = found->first + 1u;
	}
      else if (state == LookingForAddress)
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
	  auto found = bits.scan_for(thisbit, 0xF57E, 0xFFFF);
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
	  byte id[7] = {byte(id_address_mark)};
	  // Contents of the address
	  // byte 0 - mark (data, 0xFE)
	  // byte 1 - cylinder
	  // byte 2 - head (side)
	  // byte 3 - record (sector, starts from 0 in Acorn)
	  // byte 4 - size code (see switch below)
	  // byte 5 - CRC byte 1
	  // byte 6 - CRC byte 2
	  if (!bits.copy_fm_bytes(thisbit, 6u, &id[1], verbose_))
	    {
	      if (verbose_)
		{
		  std::cerr << "Failed to read sector address\n";
		}
	      state = Desynced;
	      continue;
	    }
	  DFS::CCITT_CRC16 crc;
	  crc.update(id, id + sizeof(id));
	  const auto addr_crc = crc.get();
	  if (addr_crc)
	    {
	      if (verbose_)
		{
		  std::cerr << "Sector address CRC mismatch: 0x"
			    << std::hex << addr_crc << " should be 0\n";
		}
	      state = Desynced;
	      continue;
	    }

	  const byte* addr = &id[1];
	  sec.address.cylinder = id[1];
	  sec.address.head = id[2];
	  sec.address.record = id[3];
	  std::optional<int> ssiz = decode_sector_size(id[4]);
	  if (!ssiz)
	    {
	      if (verbose_)
		{
		  std::cerr << "saw unexpected sector size code " << addr[3] << "\n";
		}
	      sec_size = addr[3];
	      state = Desynced;
	      continue;
	    }
	  sec_size = *ssiz;
	  // id[5] and id[6] are the CRC bytes, and these already got
	  // included in our evaluation of addr_crc.
	  state = LookingForRecord;
	}
      else if (state == LookingForRecord)
	{
	  std::optional<std::pair<size_t, unsigned int>> found = find_record_address_mark();
	  if (!found)
	    break;
	  thisbit = found->first;
	  const bool discard_record = (found->second & 0xFFFF) == 0xF56A;
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
	  auto size_with_crc = sec_size + 2;  // add two bytes for the CRC
	  sec.data.resize(size_with_crc);
	  byte data_mark[1] =
	    {
	     byte(discard_record ? deleted_data_address_mark : data_address_mark)
	    };
	  if (!bits.copy_fm_bytes(thisbit, size_with_crc, sec.data.data(), verbose_))
	    {
	      if (verbose_)
		{
		  std::cerr << "Lost sync in sector data\n";
		}
	      state = Desynced;
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
		}
	      state = Desynced;
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
	  state = Desynced;
	}
    }
  return result;
}

}  // namespace Track
