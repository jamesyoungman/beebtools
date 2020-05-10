set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(dfsbase OBJECT)
target_sources(dfsbase
  PRIVATE
  stringutil.cc
  stringutil.h)
  
add_executable(dfs)
install(TARGETS dfs
  RUNTIME
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

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
  cmd_dump.cc
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

######################################## Unit Tests

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

######################################## Regression Tests

file(GLOB SH_TEST_SCRIPTS tests/test_*.sh)
set(SH_TEST_CWD ${CMAKE_CURRENT_BINARY_DIR}/work)
file(MAKE_DIRECTORY ${SH_TEST_CWD})

foreach(SH_TEST_SCRIPT ${SH_TEST_SCRIPTS})
  file(RELATIVE_PATH shortname ${CMAKE_CURRENT_SOURCE_DIR} ${SH_TEST_SCRIPT})
  add_test(
    NAME ${shortname}
    COMMAND ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/test_runner.sh
    ${BOURNE_SHELL} ${SH_TEST_SCRIPT} $<TARGET_FILE:dfs> ${CMAKE_CURRENT_SOURCE_DIR}/testdata 
    WORKING_DIRECTORY ${SH_TEST_CWD})
endforeach()
