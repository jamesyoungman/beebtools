#ifndef INC_CLEANUP_H
#define INC_CLEANUP_H 1

#include <functional>

class cleanup
{
 public:
  cleanup(std::function<void()> cleaner)
    : run_(cleaner), running_(false) {}

  ~cleanup()
    {
      if (!running_)
	{
	  running_ = true;
	  run_();
	}
    }

 private:
  std::function<void()> run_;
  bool running_;
};


#endif
