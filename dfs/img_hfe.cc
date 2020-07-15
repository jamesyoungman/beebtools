/*
  HFE file format support
  Based on Rev.1.1 - 06/20/2012 of the format specification.
  Documentation: https://hxc2001.com/download/floppy_drive_emulator/SDCard_HxC_Floppy_Emulator_HFE_file_format.pdf
*/
#include <array>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>

#include "abstractio.h"
#include "dfs.h"
#include "exceptions.h"
#include "hexdump.h"
#include "identify.h"
#include "media.h"
#include "track.h"

namespace
{
using byte = unsigned char;

/* reverse the ordering of bits in a byte. */
inline byte reverse_bit_order(byte in)
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


class InvalidHfeFile : public std::exception
{
public:
  InvalidHfeFile(const std::string& msg)
    : msg_(msg)
  {
  }

  const char *what() const noexcept override
  {
    return msg_.c_str();
  }

private:
  std::string msg_;
};

class UnsupportedHfeFile : public std::exception
{
public:
  UnsupportedHfeFile(const std::string& msg)
    : msg_(msg)
  {
  }

  const char *what() const noexcept override
  {
    return msg_.c_str();
  }

private:
  std::string msg_;
};

#define IBMPC_DD_FLOPPYMODE                  0x00
#define IBMPC_HD_FLOPPYMODE		     0x01
#define ATARIST_DD_FLOPPYMODE		     0x02
#define ATARIST_HD_FLOPPYMODE		     0x03
#define AMIGA_DD_FLOPPYMODE		     0x04
#define AMIGA_HD_FLOPPYMODE		     0x05
#define CPC_DD_FLOPPYMODE		     0x06
#define GENERIC_SHUGGART_DD_FLOPPYMODE	     0x07
#define IBMPC_ED_FLOPPYMODE		     0x08
#define MSX2_DD_FLOPPYMODE		     0x09
#define C64_DD_FLOPPYMODE		     0x0A
#define EMU_SHUGART_FLOPPYMODE		     0x0B
#define S950_DD_FLOPPYMODE		     0x0C
#define S950_HD_FLOPPYMODE		     0x0D
#define DISABLE_FLOPPYMODE		     0xFE
#define ISOIBM_MFM_ENCODING                  0x00
#define AMIGA_MFM_ENCODING		     0x01
#define ISOIBM_FM_ENCODING		     0x02
#define EMU_FM_ENCODING			     0x03
#define UNKNOWN_ENCODING		     0xFF

struct picfileformatheader
{
  unsigned char HEADERSIGNATURE[8];
  unsigned char formatrevision;
  unsigned char number_of_track;
  unsigned char number_of_side;
  unsigned char track_encoding;
  unsigned short bitRate;
  unsigned short floppyRPM;
  unsigned char floppyinterfacemode;
  unsigned char dnu;
  unsigned short track_list_offset;
  unsigned char write_allowed;
  unsigned char single_step;
  unsigned char track0s0_altencoding;
  unsigned char track0s0_encoding;
  unsigned char track0s1_altencoding;
  unsigned char track0s1_encoding;
};

unsigned short le_word(const unsigned char* p)
{
  return static_cast<unsigned short>(p[0] | (p[1] << 8u));
}

picfileformatheader decode_header(const unsigned char *d)
{
  auto nextbyte = [&d]() -> unsigned char
		  {
		    return *d++;
		  };
  auto nextshort = [&d]() -> unsigned short
		   {
		     auto val = le_word(d);
		     d += 2;
		     return val;
		   };

  picfileformatheader h;
  std::copy(d, d+sizeof(h.HEADERSIGNATURE), h.HEADERSIGNATURE);
  std::advance(d, sizeof(h.HEADERSIGNATURE));
  assert(sizeof(h.HEADERSIGNATURE) == 8);
  /* 0x00 ... 0x07 is HEADERSIGNATURE, above. */
  /* 0x08 */ h.formatrevision = nextbyte();
  /* 0x09 */ h.number_of_track = nextbyte();
  /* 0x0A */ h.number_of_side = nextbyte();
  /* 0x0B */ h.track_encoding = nextbyte();
  /* 0x0C */ h.bitRate = nextshort();
  /* 0x0D - second byte of bitRate */
  /* 0x0E */ h.floppyRPM = nextshort();
  /* 0x0F - second byte of floppyRPM */
  /* 0x10  */ h.floppyinterfacemode = nextbyte();
  /* 0x11  */ h.dnu = nextbyte();
  /* 0x12  */ h.track_list_offset = nextshort();
  /* 0x13 - second byte of track_list_offset */
  /* 0x14 */ h.write_allowed = nextbyte();
  /* 0x15 */ h.single_step = nextbyte();
  /* 0x16 */ h.track0s0_altencoding = nextbyte();
  /* 0x17 */ h.track0s0_encoding = nextbyte();
  /* 0x18 */ h.track0s1_altencoding = nextbyte();
  /* 0x19 */ h.track0s1_encoding = nextbyte();
  return h;
}

const char* step_mode(unsigned char val)
{
  switch (val)
    {
    case 0xFF:
      return "single step";
    case 0x00:
      return "double step";
    default:
      return "unknown step mode";
    }
}

const char *encoding_name(unsigned char val)
{
  switch (val)
    {
    case ISOIBM_MFM_ENCODING:
      return "ISO/IBM MFM";
    case AMIGA_MFM_ENCODING:
      return "Amiga MFM";
    case ISOIBM_FM_ENCODING:
      return "ISO/IBM FM";
    case EMU_FM_ENCODING:
      return "EMU FM";
    case UNKNOWN_ENCODING:
    default:
      return "unknown";
    }
}

const char * alt_encoding(unsigned char val)
{
  return val == 0x00 ? "yes" : "no";
}

std::ostream& operator<<(std::ostream& os, const picfileformatheader& h)
{
  std::ostream::sentry s(os);
  if (s)
    {
      os << "HEADERSIGNATURE         " << h.HEADERSIGNATURE << "\n";
      os << "formatrevision          " << static_cast<unsigned int>(h.formatrevision) << "\n";
      os << "number_of_track         " << static_cast<unsigned int>(h.number_of_track) << "\n";
      os << "number_of_side          " << static_cast<unsigned int>(h.number_of_side) << "\n";
      os << "track_encoding          " << static_cast<unsigned int>(h.track_encoding)
	 << " = " << encoding_name(h.track_encoding) << "\n";
      os << "bitRate                 " << static_cast<unsigned int>(h.bitRate) << "\n";
      os << "floppyRPM               " << static_cast<unsigned int>(h.floppyRPM) << "kbit/s\n";
      os << "floppyinterfacemode     " << static_cast<unsigned int>(h.floppyinterfacemode) << "\n";
      os << "dnu                     " << static_cast<unsigned int>(h.dnu) << "\n";
      os << "track_list_offset       " << h.track_list_offset
	 << " = " << (h.track_list_offset * 512) << " bytes\n";
      os << "write_allowed           " << std::boolalpha << bool(h.write_allowed) << "\n";
      os << "single_step             " << step_mode(h.single_step) << "\n";
      os << "track0s0_altencoding    " << alt_encoding(h.track0s0_altencoding) << "\n";
      os << "track0s0_encoding       "
	 << static_cast<unsigned int>(h.track0s0_encoding)
	 << " => " << encoding_name(h.track0s0_altencoding == 0 ?
				    h.track0s0_encoding : h.track_encoding) << "\n";
      os << "track0s1_altencoding    " << alt_encoding(h.track0s1_altencoding) << "\n";
      os << "track0s1_encoding       "
	 << static_cast<unsigned int>(h.track0s1_encoding)
	 << " => " << encoding_name(h.track0s1_altencoding == 0 ?
				    h.track0s1_encoding : h.track_encoding) << "\n";
    }
  return os;
}

struct PicTrack
{
public:
  PicTrack(const unsigned char* p)
    : offset(le_word(p)),
      track_len(le_word(p+2))
  {
  }

  unsigned short offset;
  unsigned short track_len;
};

std::vector<PicTrack>
read_track_offset_lut(std::ifstream& f, unsigned int tracks)
{
  std::vector<unsigned char> buf;
  std::vector<PicTrack> result;
  buf.resize(tracks * 4u);
  f.seekg(512);
  f.read(reinterpret_cast<char*>(buf.data()), buf.size());
  const unsigned char *pos = buf.data();
  for (unsigned  i = 0; i < tracks; ++i)
    {
      result.push_back(PicTrack(pos));
      pos += 4u;
    }
  return result;
}


class HfeFile : public DFS::AbstractImageFile
{
public:
  explicit HfeFile(const std::string& name);

  class DataAccessAdapter : public DFS::AbstractDrive
  {
  public:
    DataAccessAdapter(HfeFile* f,
		      DFS::Geometry geom,
		      unsigned int side,
		      const std::vector<Sector> sectors)
      : f_(f),
	geom_(geom),
	side_(side),
	sectors_(sectors)
    {
    }

    std::optional<DFS::SectorBuffer> read_block(unsigned long lba) override
    {
      if (lba >= sectors_.size())
	return std::nullopt;
      const Sector& sect(sectors_[lba]);
      DFS::SectorBuffer buf;
      std::copy(sect.data.begin(), sect.data.end(), buf.begin());
      return buf;
    }

    std::string description() const
    {
      std::ostringstream ss;
      ss << "side " << side_ << " of " << f_->description();
      return ss.str();
    }

    DFS::Geometry geometry() const
    {
      return geom_;
    }

  private:
    HfeFile *f_;
    DFS::Geometry geom_;  // geom has just one side.
    unsigned int side_;
    std::vector<Sector> sectors_;
  };

  std::string description() const;
  bool connect_drives(DFS::StorageConfiguration* storage, DFS::DriveAllocation how,
		      std::string& error) override;

private:
  SectorAddress lba_to_address(unsigned long lba);

  std::vector<Sector> read_all_sectors(std::ifstream& f,
				       const std::vector<PicTrack>& lut,
				       unsigned int side);
  std::string name_;
  std::ifstream f;
  picfileformatheader header_;
  DFS::Geometry geom_;
  std::vector<DataAccessAdapter> acc_;
};

HfeFile::HfeFile(const std::string& name)
  : name_(name),
    f(name, std::ifstream::binary)
{
  if (!f)
    throw DFS::FileIOError(name, errno);
  f.exceptions(std::ios::badbit);
  try
    {
      unsigned char header_data[512];
      f.read(reinterpret_cast<char*>(header_data), sizeof(header_data));
      header_ = decode_header(header_data);

      if (DFS::verbose)
	{
	  std::cerr << name << ":\n" << header_ << "\n";
	}
      if (memcmp(header_.HEADERSIGNATURE, "HXCPICFE", 8))
	{
	  throw InvalidHfeFile("invalid header signature");
	}

      if (header_.track_encoding != ISOIBM_FM_ENCODING &&
	  header_.track_encoding != ISOIBM_MFM_ENCODING)
	{
	  std::ostringstream ss;
	  ss << "unsupported track_encoding value " << header_.track_encoding;
	  throw UnsupportedHfeFile(ss.str());
	}
      std::vector<PicTrack> track_lut = read_track_offset_lut(f, header_.number_of_track);

      for (unsigned int side = 0; side < header_.number_of_side; ++side)
	{
	  std::vector<Sector> sectors = read_all_sectors(f, track_lut, side);
	  DFS::Geometry geom = geom_;
	  geom.heads = 1;
	  acc_.emplace_back(this, geom, side, sectors);
	}
    }
  catch (std::ifstream::failure& e)
    {
      // TODO: I don't think we can get a real errno value here.
      throw DFS::FileIOError(name, EIO);
    }
}

std::string HfeFile::description() const
{
  std::ostringstream ss;
  ss << "HFE file " << name_;
  return ss.str();
}

void copy_hfe(const byte* begin, const byte* end,
	      std::back_insert_iterator<std::vector<byte>> dest)
{
  int take_this_bit = 0;
  int got_bits = 0;
  while (begin != end)
    {
      byte out, in = *begin++;
      for (int bitnum = 0; bitnum < 8; ++bitnum)
	{
	  if (take_this_bit)
	    {
	      const int bit = in & (1 << (7-bitnum)) ? 0x80 : 0;
	      /* the output bit might be a clock bit or it might be
		 data, we worry about that separately. */
	      out = static_cast<byte>((out >> 1 ) | bit);
	      ++got_bits;
	    }
	  take_this_bit = !take_this_bit;
	}
      if (8 == got_bits)
	{
	  *dest++ = out;
	  got_bits = 0;
	}
    }
}

std::vector<Sector>
HfeFile::read_all_sectors(std::ifstream& f,
			  const std::vector<PicTrack>& lut,
			  unsigned int side)
{
  assert(side == 0 || side == 1);
   // offset_unit_size is the unit size of lut[i].offset_in_blocks.
  constexpr unsigned int offset_unit_size = 512;
  std::vector<Sector> result;
  std::optional<unsigned int> sectors_per_track;
  for (unsigned int track = 0 ; track < header_.number_of_track; ++track)
    {
      unsigned short offset_in_blocks = lut[track].offset;
      unsigned long track_len_in_bytes = lut[track].track_len;

      constexpr unsigned int side_block_size = 256;
      constexpr unsigned int raw_data_block_size = side_block_size * 2;
      const auto max_offset = std::numeric_limits<std::streamoff>::max();
      assert(max_offset / raw_data_block_size > offset_in_blocks);

      std::vector<byte> raw_data;
      raw_data.resize(track_len_in_bytes);
      f.seekg(offset_in_blocks * static_cast<std::streampos>(offset_unit_size), f.beg);
      f.read(reinterpret_cast<char*>(raw_data.data()), track_len_in_bytes);
      auto track_bytes_read = static_cast<unsigned int>(f.gcount());
      if (DFS::verbose)
	{
	  std::cerr << "Track " << track << " has " << track_len_in_bytes
		    << " bytes of data; we read " << track_bytes_read
		    << "\n";
	}
      // While we could simply deal with the bit ordering in the input file when
      // dealing with subsequent stages, that will make it harder to interpret
      // nuumeric arguments to HFEv3 opcodes.
      assert(reverse_bit_order(0x43) == 0xC2);
      std::transform(raw_data.begin(), raw_data.end(),
		     raw_data.begin(), // transform in-place.
		     reverse_bit_order);

      // The data is in side_block_size chunks (side 0 then side 1,
      // etc.) but we only want the data for one of the sides.
      std::vector<byte> track_stream;
      track_stream.reserve(track_len_in_bytes / 2);
      auto begin_offset = side_block_size * side;
      while (begin_offset < track_bytes_read)
	{
	  const auto end_offset = std::min(begin_offset + side_block_size,
					   track_bytes_read);
	  assert(end_offset <= raw_data.size());
	  if (DFS::verbose)
	    {
	      std::cerr << "Track " << track << ": copying "
			<< (end_offset - begin_offset) << " bytes starting at "
			<< "offset " << begin_offset << " to position "
			<< track_stream.size() << " in the track stream\n";
	      std::cerr << "Input:\n";
	      DFS::hexdump_bytes(std::cerr, track_stream.size(),
				 (end_offset - begin_offset),
				 16, raw_data.data() + begin_offset);
	    }
	  auto oldsize = track_stream.size();
	  copy_hfe(raw_data.data() + begin_offset,
		   raw_data.data() + end_offset,
		   std::back_inserter(track_stream));
	  if (DFS::verbose)
	    {
	      std::cerr << "Output:\n";
	      DFS::hexdump_bytes(std::cerr, oldsize,
				 track_stream.size() - oldsize, 16,
				 track_stream.data() + oldsize);
	    }
	  begin_offset += raw_data_block_size;
	}
      if (DFS::verbose)
	{
	  std::cerr << std::dec << std::setfill(' ')
		    << "Track " << std::setw(2) << track << ": " << track_len_in_bytes
		    << " bytes at position "
		    << (offset_unit_size * offset_in_blocks)
		    << "; " << track_stream.size()
		    << " bytes seem to be for side " << side << "\n";
	}


      // Extract the FM-encoded sectors.
      std::vector<Sector> track_sectors =  IbmFmDecoder(DFS::verbose).decode(track_stream);
      if (DFS::verbose)
	{
	  std::cerr << "Found " << track_sectors.size() << " sectors on track "
		    << track << "\n";
	}
      if (!sectors_per_track)
	{
	  sectors_per_track = track_sectors.size();
	}
      else if (track_sectors.size() != *sectors_per_track)
	{
	  std::ostringstream ss;
	  ss << "track " << track << " has " << track_sectors.size()
	     << " sectors but other tracks have " << *sectors_per_track
	     << " sectors; this is not supported";
	  throw UnsupportedHfeFile(ss.str());
	}

      // Sort the sectors by address.
      std::sort(track_sectors.begin(), track_sectors.end(),
		[](const Sector& a, const Sector& b)
		{
		  return a.address < b.address;
		});

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
	      throw UnsupportedHfeFile(ss.str());
	    }
	  if (sect.address.cylinder != track)
	    {
	      ss << "found sector with address " << sect.address
		 << " in the data for track " << track;
	      throw UnsupportedHfeFile(ss.str());
	    }
	  if (prev_rec_num)
	    {
	      if (*prev_rec_num == sect.address.record)
		{
		  ss << "sector with address " << sect.address
		     << " has a duplicate record number ";
		  throw UnsupportedHfeFile(ss.str());
		}
	      else if (*prev_rec_num + 1 < sect.address.record)
		{
		  ss << "before sector with address " << sect.address
		     << " there is no sector with record number "
		     << (*prev_rec_num + 1);
		  throw UnsupportedHfeFile(ss.str());
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
		  if (DFS::verbose)
		    {
		      std::cerr << "warning: the lowest-numbered sector of "
				<< "track " << track << " has address "
				<< sect.address
				<< " but it should have record number 0 "
				<< "instead of "
				<< unsigned(sect.address.record);
		    }
		}
	    }

	  if (sect.data.size() != DFS::SECTOR_BYTES)
	    {
	      ss << "track " << track
		 << " contains a sector with address " << sect.address
		 << " but it has unsupported size " << sect.data.size();
	      throw UnsupportedHfeFile(ss.str());
	    }

	  *prev_rec_num = sect.address.record;
	}
      // TODO: check the CRC
      std::copy(track_sectors.begin(), track_sectors.end(),
		std::back_inserter(result));
    }

  assert(header_.number_of_track > 0);
  assert(sectors_per_track.has_value());
  DFS::Encoding enc;
  switch (header_.track_encoding)
    {
    case ISOIBM_MFM_ENCODING:
    case AMIGA_MFM_ENCODING:
      enc = DFS::Encoding::MFM;
      break;
    case ISOIBM_FM_ENCODING:
    case EMU_FM_ENCODING:
      enc = DFS::Encoding::FM;
      break;
    default:
      {
	std::ostringstream ss;
	ss << "disc has unsupported encoding "
	   << encoding_name(header_.track_encoding);
	throw UnsupportedHfeFile(ss.str());
      }
    }

  geom_ = DFS::Geometry(header_.number_of_track,
			header_.number_of_side,
			*sectors_per_track,
			enc);
  return result;
}

bool HfeFile::connect_drives(DFS::StorageConfiguration* storage,
			     DFS::DriveAllocation how,
			     std::string& error)
{
  std::vector<std::optional<DFS::DriveConfig>> drives;
  for (auto& accessor : acc_)
    {
      std::optional<DFS::Format> fmt =
	DFS::identify_file_system(accessor, accessor.geometry(), false, error);
      if (!fmt)
	return false;
      // TODO: detect unformatted drive (relevant because side 1 may be absent).
      //
      // TODO: decide how many devices to present when sides=2, presumably
      // based on the value of fmt, and bear this in mind when converting
      // the lba value in read_block back onto a track, side and sector number.
      DFS::DriveConfig dc(*fmt, &accessor);
      drives.push_back(dc);
    }
  return storage->connect_drives(drives, how);
}

}  // namespace

namespace DFS
{
  std::unique_ptr<DFS::AbstractImageFile>
  make_hfe_file(const std::string& name, std::string& error)
  {
    try
      {
	std::unique_ptr<HfeFile> result = std::make_unique<HfeFile>(name);
	return result;
      }
    catch (std::exception& e)
      {
	error = e.what();
	return 0;
      }
  }
}  // namespace DFS
