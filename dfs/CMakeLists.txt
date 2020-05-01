set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(dfsbase OBJECT)
target_sources(dfsbase
  PRIVATE
  stringutil.cc
  stringutil.h)
  
add_executable(dfs)

set(HEADERS
  dfscontext.h
  afsp.h
  fsp.h
  regularexpression.h
  dfsimage.h
  storage.h
  dfstypes.h
  commands.h)

target_sources(dfs
  PRIVATE
  dfs.cc
  cmd_cat.cc
  cmd_type.cc
  cmd_list.cc
  commands.cc
  dfsimage.cc
  storage.cc
  fsp.cc
  afsp.cc
  ${HEADERS}
  )
target_link_libraries(dfs dfsbase)

enable_testing()

add_executable(test_afsp)
target_sources(test_afsp
  PRIVATE
  test_afsp.cc afsp.cc
  stringutil.h
  afsp.h dfscontext.h)
target_link_libraries(test_afsp dfsbase)
add_test(NAME dfs_test_afsp_passes COMMAND test_afsp)

add_executable(test_stringutil)
target_sources(test_stringutil
  PRIVATE
  test_stringutil.cc
  stringutil.h)
target_link_libraries(test_stringutil dfsbase)
add_test(NAME dfs_test_stringutil_passes COMMAND test_stringutil)
