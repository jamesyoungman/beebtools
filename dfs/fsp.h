#ifndef INC_FSP_H
#define INC_FSP_H 1

#include <string>
#include <utility>

#include "dfscontext.h"

std::pair<char, std::string> directory_and_name_of(const DFSContext&, const std::string&);
bool case_insensitive_less(const std::string& left, const std::string& right);
bool case_insensitive_equal(const std::string& left, const std::string& right);
std::string rtrim(const std::string& input);
#endif
