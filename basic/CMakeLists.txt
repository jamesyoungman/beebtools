#
#   Copyright 2020 James Youngman
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

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

add_test(NAME basic_invariant_token_map
  COMMAND ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/invariant_token_map_test.sh $<TARGET_FILE:bbcbasic_to_text> ${CMAKE_CURRENT_SOURCE_DIR}/testdata/golden-token-map.txt)
set_property(TEST basic_invariant_token_map PROPERTY LABELS basic regression)

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
set_property(TEST basic_golden PROPERTY LABELS basic positive)

add_test(NAME basic_io_failure_test
  COMMAND
  ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/tests/io_failure_test.sh
  ${CMAKE_CURRENT_BINARY_DIR}/bbcbasic_to_text
  ${CMAKE_CURRENT_SOURCE_DIR}/testdata )
set_property(TEST basic_io_failure_test PROPERTY LABELS basic negative)

add_test(NAME basic_usage_error_test
  COMMAND
  ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/tests/usage_error_test.sh
  ${CMAKE_CURRENT_BINARY_DIR}/bbcbasic_to_text
  ${CMAKE_CURRENT_SOURCE_DIR}/testdata )
set_property(TEST basic_usage_error_test PROPERTY LABELS basic negative)

add_test(NAME basic_invalid_input_test
  COMMAND
  ${BOURNE_SHELL} ${CMAKE_CURRENT_SOURCE_DIR}/tests/invalid_input_test.sh
  ${CMAKE_CURRENT_BINARY_DIR}/bbcbasic_to_text
  ${CMAKE_CURRENT_SOURCE_DIR}/testdata )
set_property(TEST basic_invalid_input_test PROPERTY LABELS basic negative)
