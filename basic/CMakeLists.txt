###################################### Libraries

add_library(decoder OBJECT)
target_sources(decoder
  PRIVATE
   tokens.c lines.c decoder.c
   decoder.h tokens.h
   )
target_compile_options(decoder PRIVATE ${EXTRA_WARNING_OPTIONS})

###################################### Binaries

add_executable(bbcbasic_to_text bbcbasic_to_text.c)
target_compile_options(bbcbasic_to_text PRIVATE ${EXTRA_WARNING_OPTIONS})
target_link_libraries(bbcbasic_to_text decoder)
install(TARGETS bbcbasic_to_text
  RUNTIME
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###################################### Tests

enable_testing()

add_executable(test_tokens tests/test_tokens.c)
target_compile_options(test_tokens
  PRIVATE ${EXTRA_WARNING_OPTIONS} -I${CMAKE_CURRENT_SOURCE_DIR} )
target_link_libraries(test_tokens decoder)
add_test(NAME basic_tokens_test_passes COMMAND test_tokens)

add_test(NAME basic_golden
  COMMAND
  ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/run_golden_tests.sh
  ${CMAKE_CURRENT_BINARY_DIR}/bbcbasic_to_text
  ${CMAKE_CURRENT_SOURCE_DIR}/testdata )

add_test(NAME basic_io_failure_test
  COMMAND
  ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/tests/io_failure_test.sh
  ${CMAKE_CURRENT_BINARY_DIR}/bbcbasic_to_text
  ${CMAKE_CURRENT_SOURCE_DIR}/testdata )
