add_library(decoder OBJECT)
target_sources(decoder
  PRIVATE
   tokens.c lines.c decoder.c
   decoder.h tokens.h
   )

add_executable(bbcbasic_to_text bbcbasic_to_text.c)
target_link_libraries(bbcbasic_to_text decoder)

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
