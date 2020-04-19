#include "afsp.h"
#include <iostream>

int main() 
{
  const int rv = AFSPMatcher::SelfTest() ? 0 : 1;
  if (rv != 0) 
    {
      std::cerr << "TEST FAILURE\n";
    }
  return rv;
}


