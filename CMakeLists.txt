cmake_minimum_required(VERSION 3.10)
project(beebtools)
enable_testing()
find_program(BOURNE_SHELL sh)
set(EXTRA_WARNING_OPTIONS
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall -Wextra -Wconversion -Wpedantic>
     $<$<CXX_COMPILER_ID:MSVC>:
          /W4>)
add_subdirectory(basic)
add_subdirectory(dfs)
add_subdirectory(doc)
