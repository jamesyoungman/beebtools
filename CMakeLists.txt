#
#   Copyright 2020 James Youngman
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

cmake_minimum_required(VERSION 3.10)
project(beebtools)

option(PDF "Generate pdf format documentation" OFF)
option(COVERAGE "Measure test coverage" OFF)
option(IWYU "Perform include-what-you-use static code analysis" OFF)

if (IWYU)
  message(VERBOSE "The include-what-you-use static analyzer is enabled")
  find_program(IWYU_PATH NAMES include-what-you-use iwyu)
  if(NOT IWYU_PATH)
    message(FATAL_ERROR "Could not find the program include-what-you-use; you can disable IWYU with -DIWYU=OFF")
  endif()
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH})
  set(CMAKE_C_INCLUDE_WHAT_YOU_USE ${IWYU_PATH})
else (IWYU)
  message(VERBOSE "The include-what-you-use static analyzer is NOT enabled")
endif (IWYU)

enable_testing()
if (COVERAGE)
  # For the CodeCoverage to work, CodeCoverage.cmake needs to be in a directory
  # which is in ${CMAKE_MODULE_PATH} (which is empty by default).
  # https://github.com/bilke/cmake-modules/blob/master/CodeCoverage.cmake
  #
  # You will also need the code coverage tools, for example gcovr:
  #   sudo apt install gcovr
  include(CodeCoverage)
  append_coverage_compiler_flags()
  setup_target_for_coverage_gcovr_html(
    NAME coverage
    EXECUTABLE ctest -VV -j10
    EXCLUDE "dfs/tests/*" "basic/tests/*")
endif (COVERAGE)

find_program(BOURNE_SHELL sh)
if (${BOURNE_SHELL} STREQUAL "BOURNE_SHELL-NOTFOUND")
  message(FATAL_ERROR "Cannot find a Bourne shell 'sh'")
endif()


set(EXTRA_WARNING_OPTIONS
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall -Wextra -Wconversion -Wpedantic>
     $<$<CXX_COMPILER_ID:MSVC>:
          /W4>)
add_subdirectory(basic)
add_subdirectory(dfs)
add_subdirectory(doc)
