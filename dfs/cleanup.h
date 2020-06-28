#ifndef INC_CLEANUP_H
#define INC_CLEANUP_H 1

#include <functional>

class cleanup
{
 public:
  cleanup(std::function<void()> cleaner)
    : run_(cleaner) {}

  ~cleanup()
    {
      run_();
    }

 private:
  std::function<void()> run_;
};

// Saves and restores format flags on an ostream instance.
class ostream_flag_saver
{
public:
  ostream_flag_saver(std::ostream& os)
    : os_(os), saved_(os.flags())
  {
  }

  ~ostream_flag_saver()
  {
    os_.flags(saved_);
  }

private:
  std::ostream& os_;
  std::ostream::fmtflags saved_;
};

#endif
