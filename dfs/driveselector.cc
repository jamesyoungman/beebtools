#include "driveselector.h"

#include <optional>
#include <sstream>
#include <stdexcept>

namespace
{
  class BadSurfaceSelector : public std::invalid_argument
  {
  };
}

namespace DFS
{
  SurfaceSelector SurfaceSelector::opposite_surface() const
  {
    switch (d_ % 4)
      {
      case 0:
      case 1:
	return SurfaceSelector(d_  + 2u);
      case 2:
      case 3:
	return SurfaceSelector(d_  - 2u);
      }
    abort();
  }

  SurfaceSelector SurfaceSelector::corresponding_side_of_next_device(const SurfaceSelector& d)
  {
    auto val = d.d_ + 2u;
    if (val < d.d_)
      throw std::out_of_range("overflow in surface selector");
    return SurfaceSelector(val);
  }


  unsigned int SurfaceSelector::coerce(long int ld)
  {
    if (ld < 0)
      throw std::out_of_range("constructor value is too small");
    if (ld > std::numeric_limits<unsigned int>::max())
      throw std::out_of_range("constructor value is too large");
    return static_cast<unsigned int>(ld);
  }

  unsigned int SurfaceSelector::coerce(int i)
  {
    if (i < 0)
      throw std::out_of_range("constructor value is too small");
    if (static_cast<repr_type>(i) > std::numeric_limits<repr_type>::max())
      throw std::out_of_range("constructor value is too large");
    return static_cast<unsigned int>(i);
  }

  std::string SurfaceSelector::to_string() const
    {
      return std::to_string(d_);
    }


  SurfaceSelector SurfaceSelector::next() const
  {
    if (d_ == std::numeric_limits<unsigned int>::max())
      throw std::out_of_range("Last disc surface has no successor");
    return SurfaceSelector(d_ + 1);
  }

  SurfaceSelector SurfaceSelector::prev() const
  {
    if (d_ == 0)
      throw std::out_of_range("Surface 0 has no predecessor");
    return SurfaceSelector(d_ - 1);
  }

  void SurfaceSelector::ostream_insert(std::ostream& os) const
  {
    os << d_;
  }


  std::optional<SurfaceSelector> SurfaceSelector::parse(const std::string& s, size_t* end, std::string& error)
  {
    // If we use stoul, the input -10 is returned as a very large
    // number, insteas of having std::invalid_argument thrown.  Since
    // the error message "-10 is too large" isn't very user-friendly,
    // we use a signed conversion instead.
    long int n;
    repr_type d;
    // The error message in e.what() from stoul is pretty useless, so
    // we ignore it.
    bool use_msg = false;
    try
      {
	n = std::stol(s, end, 10);
	use_msg = true;
	d = coerce(n);
      }
    catch (BadSurfaceSelector& ebs)
      {
	error = ebs.what();
	return std::nullopt;
      }
    catch (std::out_of_range& e)
      {
	// out_of_range can be thrown by std::stod and similar.
	std::ostringstream ss;
	ss << "drive " << s << " is out of range";
	if (use_msg) ss << ": " << e.what();
	error = ss.str();
	return std::nullopt;
      }
    catch (std::invalid_argument& e)
      {
	std::ostringstream ss;
	ss << "drive " << s << " is invalid";
	if (use_msg) ss << ": " << e.what();
	error = ss.str();
	return std::nullopt;
      }
    return SurfaceSelector(d);
  }

  SurfaceSelector SurfaceSelector::acorn_default_last_surface()
  {
    return SurfaceSelector(3);
  }

  VolumeSelector::VolumeSelector(unsigned int n)
    : surface_(n)
  {
  }

  VolumeSelector::VolumeSelector(const VolumeSelector& from)
    : surface_(from.surface_), subvolume_(from.subvolume_)
  {
  }

  VolumeSelector& VolumeSelector::operator=(const VolumeSelector& v)
  {
    surface_ = v.surface_;
    subvolume_ = v.subvolume_;
    return *this;
  }

  std::string VolumeSelector::to_string() const
  {
    std::string result = surface_.to_string();
    if (subvolume_)
      result.push_back(*subvolume_);
    return result;
  }

  VolumeSelector::VolumeSelector(const SurfaceSelector& ss, char volume_label)
    : surface_(ss), subvolume_(volume_label)
  {
  }

  VolumeSelector::VolumeSelector(const SurfaceSelector& ss)
    : surface_(ss)
  {
  }

  std::optional<VolumeSelector> VolumeSelector::parse(const std::string& s, size_t*end, std::string& error)
  {
    std::optional<SurfaceSelector> ss = SurfaceSelector::parse(s, end, error);
    if (ss)
      {
	const char volume_label = s[*end];
	switch (volume_label)
	  {
	  case 'A':
	  case 'B':
	  case 'C':
	  case 'D':
	  case 'E':
	  case 'F':
	  case 'G':
	  case 'H':
	    ++*end;
	    return VolumeSelector(*ss, volume_label);

	  default:
	    return VolumeSelector(*ss);
	  }
      }
    return std::nullopt;
  }

  std::optional<char> VolumeSelector::subvolume() const
  {
    return subvolume_;
  }

  char VolumeSelector::effective_subvolume() const
  {
    return subvolume_.value_or('A');
  }

  bool VolumeSelector::operator<(const VolumeSelector& other) const
  {
    if (surface_ < other.surface_)
      return true;
    if (other.surface_ < surface_)
      return false;
    return effective_subvolume() < other.effective_subvolume();
  }

  bool VolumeSelector::operator==(const VolumeSelector& other) const
  {
    if (*this < other)
      return false;
    if (other < *this)
      return false;
    return true;
  }

  void VolumeSelector::ostream_insert(std::ostream& os) const
  {
    if (subvolume_)
      {
	std::ostringstream ss;
	// We can't just emit the parts directly because there may be
	// a field width specification in effect on os.
	ss << surface_ << *subvolume_;
	os << ss.str();
      }
    else
      {
	os << surface_;
      }
  }

}  // namespace DFS

namespace std
{
  ostream& operator<<(ostream& os, const DFS::SurfaceSelector& sel)
  {
    std::ostream::sentry s(os);
    if (s)
      {
	sel.ostream_insert(os);
      }
    return os;
  }

  ostream& operator<<(ostream& os, const DFS::VolumeSelector& vol)
  {
    std::ostream::sentry s(os);
    if (s)
      {
	vol.ostream_insert(os);
      }
    return os;
  }

  string to_string(const DFS::SurfaceSelector& sel)
  {
    return sel.to_string();
  }
}
