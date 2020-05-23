cmake_minimum_required(VERSION 3.10)
project(beebtools)
enable_testing()
if (OPT_COVERAGE)
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
    EXECUTABLE ctest -VV
    EXCLUDE "dfs/tests/*" "basic/tests/*")
endif (OPT_COVERAGE)
find_program(BOURNE_SHELL sh)
set(EXTRA_WARNING_OPTIONS
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall -Wextra -Wconversion -Wpedantic>
     $<$<CXX_COMPILER_ID:MSVC>:
          /W4>)
add_subdirectory(basic)
add_subdirectory(dfs)
add_subdirectory(doc)
