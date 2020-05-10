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

###################################### Tests

enable_testing()

add_executable(tokens_test tokens_test.c)
target_link_libraries(tokens_test decoder)
add_test(NAME basic_tokens_test_passes COMMAND tokens_test)

add_test(NAME basic_golden
  COMMAND
  ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/run_golden_tests.sh
  ${CMAKE_CURRENT_BINARY_DIR}/bbcbasic_to_text
  ${CMAKE_CURRENT_SOURCE_DIR}/testdata )

