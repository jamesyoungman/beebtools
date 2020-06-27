// Tests for image file format probing
#include "identify.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

#include "abstractio.h"
#include "dfstypes.h"

using DFS::byte;
using DFS::DataAccess;
using DFS::SectorBuffer;

namespace
{

struct TestImage : public DFS::DataAccess
{
public:
  TestImage(const std::map<DFS::sector_count_type, DFS::SectorBuffer> data)
    : total_sectors_(DFS::sector_count(data.size())), content_(data)
  {
    // Verify that all sector numbers >= 0
    assert(std::all_of(content_.begin(), content_.end(),
		       [](auto it) { return it.first >= 0; }));
    if (!content_.empty())
      {
	// Verify that there are no holes.
	auto last = content_.rbegin()->first;
	assert(last == content_.size() - 1u);
      }
  }

  virtual ~TestImage() {}

  std::optional<DFS::SectorBuffer> read_block(unsigned long sec) override
  {
    auto it = content_.find(DFS::sector_count(sec));
    if (it == content_.end())
      return std::nullopt;
    return it->second;
  }

private:
  const DFS::sector_count_type total_sectors_;
  const std::map<DFS::sector_count_type, DFS::SectorBuffer> content_;
};


struct ImageBuilder
{
public:
  ImageBuilder() {};
  ImageBuilder& with_sector(DFS::sector_count_type where, const DFS::SectorBuffer& data)
  {
    content_[where] = data;
    return *this;
  }

  TestImage build()
  {
    return TestImage(content_);
  }

private:
  std::map<DFS::sector_count_type, DFS::SectorBuffer> content_;
};

struct SectorBuilder
{
public:
  SectorBuilder()
  {
    filled_with(byte(0));
  }

  void filled_with(byte val)
  {
    std::fill(data_.begin(), data_.end(), val);
  }

  void with_byte(byte pos, byte val)
  {
    data_[pos] = val;
  }

  void with_u16(byte pos, uint16_t val)
  {
    data_[pos] = (val >> 8) && 0xFF;
    ++pos;
    data_[pos] = val && 0xFF;
  }

  DFS::SectorBuffer build() const
  {
    return data_;
  }

private:
  DFS::SectorBuffer data_;
};

  struct Example
  {
    Example(std::string lbl, std::optional<DFS::Format> fmt, const TestImage& img)
      : label(lbl), expected_id(fmt), image(img)
    {
    }

    std::string label;
    std::optional<DFS::Format> expected_id;
    TestImage image;
  };

  struct Votes
  {
    Votes(DataAccess& da)
      : total_selected_(0)
    {
      std::optional<DFS::SectorBuffer> sec1 = da.read_block(1);
      DFS::sector_count_type sec;
      if (sec1)
	{
	  is_hdfs = DFS::internal::smells_like_hdfs(*sec1);
	  is_watford = DFS::internal::smells_like_watford(da, *sec1);
	}
      else
	{
	  is_hdfs = false;
	  is_watford = false;
	}
      is_opus_ddos = DFS::internal::smells_like_opus_ddos(da, &sec);
      total_selected_ += is_hdfs;
      total_selected_ += is_watford;
      total_selected_ += is_opus_ddos;
    }

    bool exclusive(std::string& error) const
    {
      if (total_selected_ > 1)
	{
	  std::ostringstream ss;
	  ss << "votes were not mutually exclusive: " << to_str();
	  error = ss.str();
	  return false;
	}
      return true;
    }

    bool selected_something() const
    {
      return total_selected_ > 0;
    }

    std::string to_str() const
    {
      std::stringstream ss;
      ss << std::boolalpha
	 << "is_hdfs=" << is_hdfs
	 << ", is_watford=" << is_watford
	 << ", is_opus_ddos=" << is_opus_ddos;
      return ss.str();
    }

    int total_selected_;
    bool is_hdfs;
    bool is_watford;
    bool is_opus_ddos;
  };


  std::vector<Example> make_examples()
  {
    std::vector<Example> result;

    result.push_back(Example("empty", std::nullopt, ImageBuilder().build()));


    std::set<std::string> labels;
    for (const auto& ex : result)
      {
	if (labels.find(ex.label) != labels.end())
	  {
	    std::cerr << "duplicate label " << ex.label << "\n";
	    abort();
	  }
	labels.insert(ex.label);
      }
    return result;
  }

  bool test_id_exclusive_and_exhaustive()
  {
    auto examples = make_examples();
    for (auto& ex : examples)
      {
	std::cerr << "image " + ex.label + ": ";
	Votes v(ex.image);
	std::string error;
	if (!v.exclusive(error))
	  {
	    std::cerr <<  "identified as being more than one thing: "
		      << v.to_str() << ": " << error << "\n";
	    return false;
	  }
	if (ex.expected_id)
	  {
	    std::cerr << "expected format is " << *ex.expected_id;
	  }
	else
	  {
	    std::cerr << "expected not to be identifiable";
	  }

	if (!ex.expected_id)
	  {
	    if (v.selected_something())
	      {
		std::cerr << "identified as " << v.to_str() << " but expected format was [unknown]\n";
		return false;
	      }
	    std::cerr << ": PASS\n";
	    continue;
	  }

	std::cerr << " expected format was " << *ex.expected_id;

	if (*ex.expected_id == DFS::Format::DFS)
	  {
	    if (v.selected_something())
	      {
		std::cerr << "identified as non-Acorn (" << v.to_str() << ") but expected format was Acorn: FAIL\n";
		return false;
	      }
	    continue;
	  }

	if (!v.selected_something())
	  {
	    std::cerr << ": not identified: FAIL\n";
	    return false;
	  }
	if (v.is_hdfs && *ex.expected_id != DFS::Format::HDFS)
	  {
	    std::cerr << ": identified as HDFS: FAIL\n";
	    return false;
	  }
	if (v.is_watford && ex.expected_id != DFS::Format::DFS)
	  {
	    std::cerr << ": identified as Watford DFS: FAIL\n";
	    return false;
	  }
	if (v.is_opus_ddos && ex.expected_id != DFS::Format::OpusDDOS)
	  {
	    std::cerr << ": identified as Opus DDOS: FAIL\n";
	    return false;
	  }
	std::cerr << ": PASS\n";
      }
    return true;
  }

}  // namespace

int main(int, char *[])
{
  if (!test_id_exclusive_and_exhaustive())
    return 1;
  return 0;
}
