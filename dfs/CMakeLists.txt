set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package( ZLIB REQUIRED )
if ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    add_definitions(-DUSE_ZLIB)
endif( ZLIB_FOUND )

add_library(dfsbase OBJECT)
target_sources(dfsbase
  PRIVATE
  exceptions.cc
  stringutil.cc
  exceptions.h
  stringutil.h)
target_compile_options(dfsbase PRIVATE ${EXTRA_WARNING_OPTIONS})

add_executable(dfs)
install(TARGETS dfs
  RUNTIME
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
target_compile_options(dfs PRIVATE ${EXTRA_WARNING_OPTIONS})

set(HEADERS
  afsp.h
  cleanup.h
  commands.h
  crc.h
  dfscontext.h
  dfs.h
  dfs_catalog.h
  dfs_filesystem.h
  dfs_format.h
  dfs_unused.h
  dfstypes.h
  exceptions.h
  fsp.h
  media.h
  regularexpression.h
  storage.h
  stringutil.h
  )

target_sources(dfs
  PRIVATE
  crc.cc
  cmd_cat.cc
  cmd_dump.cc
  cmd_extract_files.cc
  cmd_extract_unused.cc
  cmd_free.cc
  cmd_help.cc
  cmd_info.cc
  cmd_type.cc
  cmd_list.cc
  cmd_sector_map.cc
  cmd_show_titles.cc
  cmd_space.cc
  commands.cc
  dfs_catalog.cc
  dfs_filesystem.cc
  dfs_unused.cc
  main.cc
  media.cc
  storage.cc
  fsp.cc
  afsp.cc
  ${HEADERS}
  )
if ( ZLIB_FOUND )
  target_sources(dfs PRIVATE gzmedia.cc)
  target_link_libraries(dfs ${ZLIB_LIBRARIES})
endif (ZLIB_FOUND)
target_link_libraries(dfs dfsbase)
if ( ZLIB_FOUND )
endif( ZLIB_FOUND )

######################################## Unit Tests

enable_testing()

add_executable(test_afsp)
target_sources(test_afsp
  PRIVATE
  tests/test_afsp.cc
  afsp.cc
  stringutil.h
  afsp.h dfscontext.h)
target_compile_options(test_afsp
  PRIVATE ${EXTRA_WARNING_OPTIONS}
  -I ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_afsp dfsbase)
add_test(NAME dfs_test_afsp_passes COMMAND test_afsp)
set_property(TEST dfs_test_afsp_passes PROPERTY LABELS dfs unit_test)

add_executable(test_regularexpression)
target_sources(test_regularexpression
  PRIVATE
  tests/test_regularexpression.cc
  regularexpression.h)
target_compile_options(test_regularexpression
  PRIVATE ${EXTRA_WARNING_OPTIONS}
  -I ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME dfs_test_regularexpression_passes COMMAND test_regularexpression)
set_property(TEST dfs_test_regularexpression_passes PROPERTY LABELS dfs unit_test)


add_executable(test_stringutil)
target_sources(test_stringutil
  PRIVATE
  tests/test_stringutil.cc
  stringutil.h)
target_compile_options(test_stringutil
  PRIVATE ${EXTRA_WARNING_OPTIONS}
  -I ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_stringutil dfsbase)
add_test(NAME dfs_test_stringutil_passes COMMAND test_stringutil)
set_property(TEST dfs_test_stringutil_passes PROPERTY LABELS dfs unit_test)

######################################## Regression Tests

file(GLOB SH_TEST_SCRIPTS tests/test_*.sh)
set(SH_TEST_CWD ${CMAKE_CURRENT_BINARY_DIR}/work)
file(MAKE_DIRECTORY ${SH_TEST_CWD})

function(get_test_tags filename outvar)
  file(READ ${SH_TEST_SCRIPT} body)
  set(re  "# Tags: ([^\n]*)")
  string(REGEX MATCH "${re}" taglist "${body}")
  string(REGEX MATCHALL "[^ ]+" tags "${CMAKE_MATCH_1}")
  set(${outvar} "${tags}" PARENT_SCOPE)
endfunction()

foreach(SH_TEST_SCRIPT ${SH_TEST_SCRIPTS})
  file(RELATIVE_PATH shortname ${CMAKE_CURRENT_SOURCE_DIR} ${SH_TEST_SCRIPT})
  add_test(
    NAME ${shortname}
    COMMAND ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/test_runner.sh
    ${BOURNE_SHELL} ${SH_TEST_SCRIPT} $<TARGET_FILE:dfs> ${CMAKE_CURRENT_SOURCE_DIR}/testdata
    WORKING_DIRECTORY ${SH_TEST_CWD})
  get_test_tags("${SH_TEST_SCRIPT}" tags)
  set_property(TEST ${shortname} PROPERTY LABELS dfs regression_test ${tags} )
endforeach()

configure_file(
  # This is a valid disc image.
  testdata/acorn-dfs-sd-80t-empty.ssd
  # But it's not a valid gzipped file.
  testdata/not-gzipped.ssd.gz
  COPYONLY)
add_test(
  NAME invalid_compressed_data
  # not-gzipped.ssd.gz is not a valid gzip data file, despite its
  # name.  We require this problem to be detected (which this test
  # checks) and to be certain we detect unwanted pass-through, the
  # input file happens to be a valid (but empty) disc image.
  COMMAND ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/mustfail.sh
  $<TARGET_FILE:dfs> --file testdata/not-gzipped.ssd.gz cat)
  set_property(TEST invalid_compressed_data PROPERTY LABELS dfs regression_test)
