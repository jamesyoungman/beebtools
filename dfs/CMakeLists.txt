set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package( ZLIB REQUIRED )
if ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    add_definitions(-DUSE_ZLIB)
endif( ZLIB_FOUND )

set(DFSBASE_HEADERS
  exceptions.h
  stringutil.h
  dfs.h
  )

add_library(dfsbase OBJECT)
target_sources(dfsbase
  PRIVATE
  exceptions.cc
  stringutil.cc
  verbose.cc
  ${DFSBASE_HEADERS}
  )
target_compile_options(dfsbase PRIVATE ${EXTRA_WARNING_OPTIONS})

add_library(dfslib OBJECT)

set(DFSLIB_HEADERS
  # base stuff
  cleanup.h
  dfstypes.h
  # command infrastructure
  commands.h
  # Disc image file handling
  img_fileio.h
  img_sdf.h
  # File format and geometry probing
  # I/O machinery
  abstractio.h
  geometry.h
  media.h
  storage.h
  # File System implementation
  dfs_catalog.h
  dfs_filesystem.h
  dfs_format.h
  dfs_unused.h
  dfs_volume.h
  opus_cat.h
  # UI
  dfscontext.h
  driveselector.h
  afsp.h
  fsp.h
  # Other
  crc.h
  regularexpression.h
  )

target_sources(dfslib
  PRIVATE
  # Disc image file handling
  img_fileio.cc
  img_hfe.cc
  img_mmb.cc
  img_sdf.cc
  img_load.cc
  # File format and geometry probing
  identify.cc
  # I/O machinery
  storage.cc
  driveselector.cc
  geometry.cc
  # File System implementation
  dfs_catalog.cc
  dfs_filesystem.cc
  dfs_unused.cc
  dfs_volume.cc
  opus_cat.cc
  # Filename and wildcard implementation
  fsp.cc
  afsp.cc
  # UI-ish
  crc.cc  # used to build standard archive files.
  # Header files.
  ${DFSBASE_HEADERS}
  ${DFSLIB_HEADERS}
  )
if ( ZLIB_FOUND )
  target_sources(dfslib PRIVATE img_gzfile.cc)
endif (ZLIB_FOUND)
target_compile_options(dfslib PRIVATE ${EXTRA_WARNING_OPTIONS})

add_executable(dfs)
install(TARGETS dfs
  RUNTIME
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
target_compile_options(dfs PRIVATE ${EXTRA_WARNING_OPTIONS})

target_sources(dfs
  PRIVATE
  # command infrastructure
  commands.cc
  # commands
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
  # main function
  main.cc
  ${DFSBASE_HEADERS}
  ${DFSLIB_HEADERS}
  )
if ( ZLIB_FOUND )
  target_link_libraries(dfs ${ZLIB_LIBRARIES})
endif (ZLIB_FOUND)
target_link_libraries(dfs dfslib dfsbase)
if ( ZLIB_FOUND )
endif( ZLIB_FOUND )

######################################## Unit Tests

enable_testing()

add_executable(test_afsp)
target_sources(test_afsp PRIVATE tests/test_afsp.cc ${DFSBASE_HEADERS} ${DFSLIB_HEADERS})
if ( ZLIB_FOUND )
  target_link_libraries(test_afsp ${ZLIB_LIBRARIES})
endif (ZLIB_FOUND)
target_compile_options(test_afsp
  PRIVATE ${EXTRA_WARNING_OPTIONS}
  -I ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_afsp dfsbase dfslib)
add_test(NAME dfs_test_afsp_passes COMMAND test_afsp)
set_property(TEST dfs_test_afsp_passes PROPERTY LABELS dfs unit_test)

add_executable(test_geometry)
target_sources(test_geometry
  PRIVATE
  tests/test_geometry.cc
  geometry.cc
  geometry.h
  ${DFSBASE_HEADERS} ${DFSLIB_HEADERS})
target_compile_options(test_geometry
  PRIVATE ${EXTRA_WARNING_OPTIONS}
  -I ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME dfs_test_geometry_passes COMMAND test_geometry)
set_property(TEST dfs_test_geometry_passes PROPERTY LABELS dfs unit_test)

add_executable(test_prober)
target_sources(test_prober
  PRIVATE
  tests/test_prober.cc
  ${DFSBASE_HEADERS} ${DFSLIB_HEADERS})
target_compile_options(test_prober
  PRIVATE ${EXTRA_WARNING_OPTIONS}
  -I ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_prober dfslib dfsbase ${ZLIB_LIBRARIES})
add_test(NAME dfs_test_prober_passes COMMAND test_prober)
set_property(TEST dfs_test_prober_passes PROPERTY LABELS dfs unit_test)

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


add_executable(test_fileio)
target_sources(test_fileio
  PRIVATE
  tests/test_fileio.cc
  ${HEADERS})
target_compile_options(test_fileio
  PRIVATE ${EXTRA_WARNING_OPTIONS}
  -I ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_fileio dfslib dfsbase)
if ( ZLIB_FOUND )
  target_link_libraries(test_fileio ${ZLIB_LIBRARIES})
endif (ZLIB_FOUND)
add_test(NAME dfs_test_fileio_passes COMMAND test_fileio)
set_property(TEST dfs_test_fileio_passes PROPERTY LABELS dfs unit_test)


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
    ${BOURNE_SHELL} ${SH_TEST_SCRIPT} $<TARGET_FILE:dfs> ${CMAKE_CURRENT_SOURCE_DIR}/test_lib.sh ${CMAKE_CURRENT_SOURCE_DIR}/testdata
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
