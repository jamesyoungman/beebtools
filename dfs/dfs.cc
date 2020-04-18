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
    std::string result(size, '?');
    auto begin = image.begin();
    std::transform(begin+position, begin+position+size, result.begin(), byte_to_char);
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

}


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

  inline byte opt_value() const
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

private:
  vector<byte> img_;
  Format disc_format_;
};
  



  
inline 
bool dump_catalog(const Image& image) 
{
  cout << image.title();
  if (image.disc_format() != Format::HDFS) 
    {
      cout << " ("  << std::setbase(16) << image.cycle_count() << ") FM";
    }
  cout << "\n";
  const auto opt = image.opt_value();
  cout << "Drive 0            Option " << opt << " (" << decode_opt(opt) << ")\n";
  cout << "Dir. :0.$          Lib. :0.$\n\n";
  
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


