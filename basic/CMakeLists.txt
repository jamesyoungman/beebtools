###################################### Libraries

add_library(decoder OBJECT)
target_sources(decoder
  PRIVATE
   tokens.c lines.c decoder.c
   decoder.h tokens.h
   )

###################################### Binaries

add_executable(bbcbasic_to_text bbcbasic_to_text.c)
target_link_libraries(bbcbasic_to_text decoder)
install(TARGETS bbcbasic_to_text
  RUNTIME
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###################################### Manual Pages

set(INSTALL_PREFIX_SHARE ${CMAKE_INSTALL_PREFIX}/share)
set(INSTALL_PREFIX_MAN   ${INSTALL_PREFIX_SHARE}/man)
set(MAN1_NAMES bbcbasic_to_text.1)
set(MAN1_FILES)

find_program(GZIP gzip)

macro(gzip_file SOURCE DESTINATION)
  add_custom_command(
    COMMENT "gzipping ${SOURCE} to make ${DESTINATION}"
    DEPENDS ${SOURCE}
    OUTPUT ${DESTINATION}
    COMMAND ${GZIP} -9 < ${SOURCE} > ${DESTINATION}
    VERBATIM)
endmacro(gzip_file)

foreach(m IN LISTS MAN1_NAMES)
  gzip_file(${CMAKE_CURRENT_SOURCE_DIR}/${m} ${CMAKE_CURRENT_BINARY_DIR}/${m}.gz)
  list(APPEND MAN1_FILES ${CMAKE_CURRENT_BINARY_DIR}/${m}.gz)
endforeach()
ADD_CUSTOM_TARGET(man ALL DEPENDS ${MAN1_FILES})

INSTALL(FILES ${MAN1_FILES}
  DESTINATION  ${INSTALL_PREFIX_MAN}/man1)

###################################### Tests

enable_testing()

add_executable(tokens_test tokens_test.c)
target_link_libraries(tokens_test decoder)
add_test(NAME basic_tokens_test_passes COMMAND tokens_test)

find_program(BOURNE_SHELL sh)
add_test(NAME basic_golden
  COMMAND
  ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/run_golden_tests.sh
  ${CMAKE_CURRENT_BINARY_DIR}/bbcbasic_to_text
  ${CMAKE_CURRENT_SOURCE_DIR}/testdata )

