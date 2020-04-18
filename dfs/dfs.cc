#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>

enum
  {
   SECTOR_BYTES = 256
  };


using std::cerr;
using std::cout;
using std::string;
using std::vector;

typedef unsigned char byte;
typedef vector<byte>::size_type offset;
typedef unsigned short sector_count_type; // needs 10 bits

enum class Format 
  {
   HDFS,
   DFS,
   WDFS
  };

namespace 
{
  inline static string decode_opt(byte opt) 
  {
    switch (opt) 
      {
      case 0: return "off";
      case 1: return "load";
      case 2: return "run";
      case 3: return "exec";
      }
    return "?";
  }


  inline char byte_to_char(byte b) 
  {
    return char(b);
  }

  inline char byte_to_ascii7(byte b) 
  {
    return char(b & 0x7F);
  }

  inline string extract_string(const vector<byte>& image,
			       offset position, offset size) 
  {
    std::string result;
    std::transform(image.begin()+position, image.begin()+position+size,
		   std::back_inserter(result), byte_to_char);
    return result;
  }
  
  inline offset sector(int sector_num)
  {
    return SECTOR_BYTES * sector_num;
  }

  inline Format identify_format(const vector<byte>& image)
  {
    const bool hdfs = image[0x106] & 8;
    if (hdfs) 
      return Format::HDFS;

    // Look for the Watford DFS recognition string
    // in the initial entry in its extended catalog.
    if (std::all_of(image.begin()+0x200, image.begin()+0x208,
		    [](byte b) { return b = 0xAA; })) 
      {
	return Format::WDFS;
      }

    return Format::DFS;
  }

  inline string ascii7_string(vector<byte>::const_iterator begin,
			      vector<byte>::size_type len) 
  {
    string result;
    result.reserve(len);
    std::transform(begin, begin+len, std::back_inserter(result), byte_to_ascii7);
    return result;
  }
}

class CatalogEntry
{
public:
  CatalogEntry(vector<byte>::const_iterator image_data, int slot)
   : name_(ascii7_string(image_data + slot * 0x08, 0x07)),
     directory_(image_data[slot * 0x08 + 0x100])
  {
  }
  
  string name() const 
  {
    return name_;
  }


  char directory() const 
  {
    return directory_;
  }

private:
  string name_;
  char directory_;
};


class Image
{
public:
  Image(const vector<byte> disc_image)
    : img_(disc_image), disc_format_(identify_format(disc_image))
  {
  }
  
  string title() const 
  {
    vector<byte> title_data;
    title_data.resize(12);
    std::copy(img_.begin(), img_.begin()+8, title_data.begin());
    std::copy(img_.begin() + 0x100, img_.begin()+0x104, title_data.begin() + 8);
    string result;
    result.resize(12);
    std::transform(title_data.begin(), title_data.end(), result.begin(), byte_to_ascii7);
    return result;
  }

  inline int opt_value() const
  {
    return (img_[0x106] >> 4) & 0x03;
  }
  
  inline int cycle_count() const
  {
    if (disc_format_ != Format::HDFS) 
      {
	return int(img_[0x104]);
      }
    return -1;				  // signals an error
  }

  inline Format disc_format() const { return disc_format_; }

  offset end_of_catalog() const 
  {
    return img_[0x105];
  }
  
  int catalog_entry_count() const 
  {
    return end_of_catalog() / 8;
  }

  CatalogEntry get_catalog_entry(int index) const
  {
    // TODO: bounds checking
    return CatalogEntry(img_.cbegin(), index);
  }
  

  sector_count_type disc_sector_count() const
  {
    sector_count_type result = img_[0x107];
    result |= (img_[0x106] & 3) << 8;
    if (disc_format_ == Format::HDFS) 
      {
	// http://mdfs.net/Docs/Comp/Disk/Format/DFS disagrees with
	// the HDFS manual on this (the former states both that this
	// bit is b10 of the total sector count and that it is b10 of
	// the start sector).  We go with what the HDFS manual says.
	result |= img_[0] & (1 << 7);
      }
    return result;
  }
  
private:
  vector<byte> img_;
  Format disc_format_;
};

inline bool case_insensitive_less(const string& left,
				  const string& right) 
{
  auto comp = 
    [](const unsigned char lhs,
       const unsigned char rhs)
    {
      return tolower(lhs) == tolower(rhs);
    };
  const auto result = std::mismatch(left.cbegin(), left.cend(),
				    right.cbegin(), right.cend(),
				    comp);
  if (result.second == right.cend()) 
    {
      // Mismatch because right is at end-of-string.  Therefore either
      // the strings are the same length or right is shorter.  So left
      // cannot be less.
      return false;
    }
  if (result.first == left.cend()) 
    {
      // Equal, so left cannot be less.
      return false;
    }
  return tolower(*result.first) < tolower(*result.second);
}


inline 
bool dump_catalog(const Image& image) 
{
  cout << image.title();
  if (image.disc_format() != Format::HDFS) 
    {
      cout << " ("  << std::setbase(16) << image.cycle_count()
	   << std::setbase(10) << ") FM";
    }
  cout << "\n";
  const auto opt = image.opt_value();
  cout << "Drive 0            Option "
       << opt << " (" << decode_opt(opt) << ")\n";
  cout << "Dir. :0.$          Lib. :0.$\n\n";

  const int entries = image.catalog_entry_count();
  vector<int> ordered_catalog_index;
  ordered_catalog_index.reserve(entries);
  ordered_catalog_index.push_back(0);	  // dummy for title
  for (int i = 1; i <= entries; ++i)
    ordered_catalog_index.push_back(i);

  std::sort(ordered_catalog_index.begin()+1,
	    ordered_catalog_index.end(),
	    [&image](int left, int right)  -> bool
	    {
	      return case_insensitive_less
		(image.get_catalog_entry(left).name(), 
		 image.get_catalog_entry(right).name());
	    });
	      
  bool need_newline = false;
  for (int i = 1; i <= entries; ++i)
    {
      auto entry = image.get_catalog_entry(ordered_catalog_index[i]);
      if (i & 1)
	{
	  // We index from 1 so this is the left column.
	  cout << "   " << entry.name();
	  need_newline = true;
	}
      else
	{
	  cout << std::setw(19) << entry.name();
	  cout << "\n";
	  need_newline = false;
	}
    }
  if (need_newline) 
    {
      cout << "\n";
    }
  return true;
}


bool load_image(const char *filename, vector<byte>* image) 
{
  std::ifstream infile(filename, std::ifstream::in);
  if (!infile.seekg(0, infile.end))
    return false;
  int len = infile.tellg();
  if (!infile.seekg(0, infile.beg))
    return false;
  image->resize(len);
  return infile.read(reinterpret_cast<char*>(image->data()), len).good();
}

int main (int argc, char *argv[]) 
{
  for (int i = 1; i < argc; ++i) 
    {
      vector<byte> image;
      std::cout << "Dumping from " << argv[i] << "\n";
      if (!load_image(argv[i], &image))
	{
	  cerr << "failed to dump " << argv[i] << ": " << strerror(errno) << "\n";
	  return 1;
	}
      dump_catalog(image);
    }
  return 0;
}


