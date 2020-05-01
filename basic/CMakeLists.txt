add_executable(bbcbasic_to_text bbcbasic_to_text.c tokens.c tokens.h)

enable_testing()

add_executable(tokens_test
  tokens_test.c
  tokens.c
  tokens.h)
add_test(NAME basic_tokens_test_passes COMMAND tokens_test)

find_program(BOURNE_SHELL sh)
add_test(NAME basic_golden
  COMMAND
  ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/run_golden_tests.sh
  ${CMAKE_CURRENT_BINARY_DIR}/bbcbasic_to_text
  ${CMAKE_CURRENT_SOURCE_DIR}/testdata )
