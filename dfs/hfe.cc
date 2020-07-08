/*
  HFE file format support
  Based on Rev.1.1 - 06/20/2012 of the format specification.
  Documentation: https://hxc2001.com/download/floppy_drive_emulator/SDCard_HxC_Floppy_Emulator_HFE_file_format.pdf
*/
#include <array>
#include <iostream>
#include <sstream>
#include <string>

#include "abstractio.h"
#include "dfs.h"
#include "exceptions.h"
#include "identify.h"
#include "media.h"

namespace
{

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
  h.formatrevision = nextbyte();
  h.number_of_track = nextbyte();
  h.number_of_side = nextbyte();
  h.track_encoding = nextbyte();
  h.bitRate = nextshort();
  h.floppyRPM = nextshort();
  h.floppyinterfacemode = nextbyte();
  h.dnu = nextbyte();
  h.track_list_offset = nextshort();
  h.write_allowed = nextbyte();
  h.single_step = nextbyte();
  h.track0s0_altencoding = nextbyte();
  h.track0s0_encoding = nextbyte();
  h.track0s1_altencoding = nextbyte();
  h.track0s1_encoding = nextbyte();
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
  HfeFile(const std::string& name);

  class DataAccessAdapter : public DFS::AbstractDrive
  {
  public:
    DataAccessAdapter(HfeFile* f) : f_(f) {}
    std::optional<DFS::SectorBuffer> read_block(unsigned long lba) override
    {
      return f_->read_block(lba);
    }

    std::string description() const
    {
      std::ostringstream ss;
      ss << "HFE file " << f_->name_;
      return ss.str();
    }

    DFS::Geometry geometry() const
    {
      DFS::Encoding enc = DFS::Encoding::FM;
      int sectors;
      switch (f_->header_.track_encoding)
	{
	case ISOIBM_MFM_ENCODING:
	case AMIGA_MFM_ENCODING:
	  enc = DFS::Encoding::MFM;
	  sectors = 18;		// TODO: actually could be 16.
	  break;

	case ISOIBM_FM_ENCODING:
	case EMU_FM_ENCODING:
	  enc = DFS::Encoding::FM;
	  sectors = 10;
	  break;

	case UNKNOWN_ENCODING:
	default:
	  {
	    std::ostringstream ss;
	    ss << "unsupported track_encoding value " << f_->header_.track_encoding;
	    throw UnsupportedHfeFile(ss.str());
	  }
	}
      return DFS::Geometry(f_->header_.number_of_track,
			   f_->header_.number_of_side,
			   sectors, enc);
    }

  private:
    HfeFile *f_;
  };

  bool connect_drives(DFS::StorageConfiguration* storage, DFS::DriveAllocation how,
		      std::string& error) override;

private:
  std::optional<DFS::SectorBuffer> read_block(unsigned long lba);

  std::string name_;
  std::ifstream f;
  picfileformatheader header_;
  std::vector<PicTrack> track_lut_;
  DataAccessAdapter acc_;
};

HfeFile::HfeFile(const std::string& name)
  : name_(name),
    f(name, std::ifstream::binary),
    acc_(this)
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
      track_lut_ = read_track_offset_lut(f, header_.number_of_track);
    }
  catch (std::ifstream::failure& e)
    {
      // TODO: I don't think we can get a real errno value here.
      throw DFS::FileIOError(name, EIO);
    }
}

std::optional<DFS::SectorBuffer> HfeFile::read_block(unsigned long lba)
{
  // This is the key thing to implement for the HFE format.
  std::ostringstream ss;
  ss << "read_block is not implemented yet so we can't load block " << lba;
  throw UnsupportedHfeFile(ss.str());
}

bool HfeFile::connect_drives(DFS::StorageConfiguration* storage,
			     DFS::DriveAllocation how,
			     std::string& error)
{
  const auto geom = acc_.geometry();
  std::optional<DFS::Format> fmt = DFS::identify_file_system(acc_, geom, false, error);
  if (!fmt)
    return false;
  // TODO: detect unformatted drive (relevant because side 1 may be absent).
  //
  // TODO: decide how many devices to present when sides=2, presumably
  // based on the value of fmt, and bear this in mind when converting
  // the lba value in read_block back onto a track, side and sector number.
  std::vector<std::optional<DFS::DriveConfig>> drives;
  DFS::DriveConfig dc(*fmt, &acc_);
  drives.push_back(dc);
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
