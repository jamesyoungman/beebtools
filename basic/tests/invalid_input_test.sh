#! /bin/sh
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

# ARGS:
# bbcbasic_to_text
# test data directory
BBCBASIC_TO_TEXT="$1"
TEST_DATA_DIR="$2"

prefix="invalid_input_test_${$}_"
errout="${prefix}stderr.output"
stdout="${prefix}stdout.output"
expected_out="${prefix}expected.output"
last="${prefix}last.command"

cleanup() {
    rm -f "${errout}" "${stdout}"  "${expected_out}" "${last}"
}

error() {
    echo "$@" >&2
}

fatal() {
    echo FATAL: "$@" >&2
    exit 1
}

should_fail() {
    if "$@"
    then
	error "Command $@ did not fail, but it was expected to"
	exit 1
    fi
}

expect_error() {
    regex="$1"
    shift
    show_any_actual_error() {
	if [ -s "${errout}" ]
	then
	    error "Actual error output was:"
	    cat "${errout}" >&2
	else
	    error "No error messasge was issued on stderr"
	fi
    }
    echo "$@" > "${last}"
    if "$@" 2>"${errout}"
    then
	error "$@ should have exited with a nonzero status but it didn't."
	show_any_actual_error
	return 1
    fi
    if egrep -r "${regex}" < "${errout}" >/dev/null
    then
	return 0
    fi
    error "$@ exited with a non-zero status but should also have issued an error message matching ${regex}, however it didn't"
    show_any_actual_error
    return 1
}

run_test() {
    echo
    echo "TEST CASE: $@"
    if "$@"
    then
	echo "PASS: $@"
    else
	echo "FAIL: $@"
	return 1
    fi
}

missing_eof_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=SDL "${TEST_DATA_DIR}"/invalid/SDL/end.missing-eof.bbc
}

premature_eof_1_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=SDL "${TEST_DATA_DIR}"/invalid/Z80/premature-eof-1.bbc
}

premature_eof_2_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=SDL "${TEST_DATA_DIR}"/invalid/Z80/premature-eof-2.bbc
}

premature_eof_3_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=SDL "${TEST_DATA_DIR}"/invalid/Z80/premature-eof-3.bbc
}

premature_eof_4_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=6502 "${TEST_DATA_DIR}"/invalid/6502/premature-eof-4.bbc
}

premature_eof_5_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=6502 "${TEST_DATA_DIR}"/invalid/6502/premature-eof-5.bbc
}

premature_eof_6_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=6502 "${TEST_DATA_DIR}"/invalid/6502/premature-eof-6.bbc
}

premature_eof_7_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=6502 "${TEST_DATA_DIR}"/invalid/6502/premature-eof-7.bbc
}

premature_eof_8_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=6502 "${TEST_DATA_DIR}"/invalid/6502/premature-eof-8.bbc
}

premature_eof_9_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=6502 "${TEST_DATA_DIR}"/invalid/6502/premature-eof-9.bbc
}

incomplete_eof_1_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=Z80 "${TEST_DATA_DIR}"/invalid/Z80/incomplete-eof-marker-1.bbc
}

incomplete_eof_2_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=Z80 "${TEST_DATA_DIR}"/invalid/Z80/incomplete-eof-marker-2.bbc
}

eof_in_line_num_1_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=Z80 "${TEST_DATA_DIR}"/invalid/Z80/eof-in-line-num-1.bbc
}

eof_in_line_num_2_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=Z80 "${TEST_DATA_DIR}"/invalid/Z80/eof-in-line-num-2.bbc
}

le_short_line_test() {
    expect_error "line.*length.*short" "${BBCBASIC_TO_TEXT}" --dialect=SDL ${TEST_DATA_DIR}/invalid/Z80/short-line-1.bbc
}

be_short_line_test() {
    expect_error "line.*length.*short" "${BBCBASIC_TO_TEXT}" --dialect=6502 "${TEST_DATA_DIR}"/invalid/6502/short-line-2.bbc
}

eol_in_line_number_test() {
    expect_error "end-of-line in the middle of a line number" \
		 "${BBCBASIC_TO_TEXT}" --dialect=6502 \
		 "${TEST_DATA_DIR}/invalid/6502/goto-premature-eol.bbc"
}

bad_c6_ext_map_test() {
    expect_error "sequence.*0xC6.*are you sure you specified the right dialect[?]" \
		 "${BBCBASIC_TO_TEXT}" --dialect=ARM \
		 "${TEST_DATA_DIR}/invalid/ARM/bad-c6-ext-map.bbc"
}

eol_in_extension_token_test() {
    expect_error "Unexpected end-of-line immediately after token 0xC7" \
		 "${BBCBASIC_TO_TEXT}" --dialect=ARM \
		 "${TEST_DATA_DIR}/invalid/ARM/premature-eol-after-c7.bbc"
}

fastvar_test() {
    expect_error "crunched.*this tool on the original source code" \
		 "${BBCBASIC_TO_TEXT}" --dialect=SDL \
		 "${TEST_DATA_DIR}/invalid/SDL/sdl-fastvar.bbc"
}

be_incorrect_line_start_test() {
    expect_error "line at position 27 did not start with 0x0D [(]instead 0xD0[)]" \
		 "${BBCBASIC_TO_TEXT}" --dialect=6502 \
		 "${TEST_DATA_DIR}/invalid/6502/incorrect-line-start.bbc"
}

z80_trailing_junk_test() {
    # This test is unusual since the input is in some sense incorrect
    # we (we think) we see this kind of input in the wild, with other
    # characters in the file after the logical EOF marker.
    rv=4  # 4 signals that we forgot to set a proper value.
    if "${BBCBASIC_TO_TEXT}" \
	   --dialect=Z80 \
	   "${TEST_DATA_DIR}"/inputs/Z80/trailing-junk.bbc \
	   >"${stdout}" 2>"${errout}"
    then
	printf '   10 CLS\n' > "${expected_out}"
	if ! diff -u "${expected_out}" "${stdout}" >&2
	then
	    echo "FAIL: incorrect output on stdout" >&2
	    rv=1
	fi
	wanted='expected end-of-file'
	if ! grep -F -q "${wanted}" "${errout}"
	then
	    echo "FAIL: expected to see ${wanted} in the stderr output" >&2
	    echo "instead, got this:" >&2
	    cat < "${errout}" >&2
	    rv=1
	fi
	echo "z80_trailing_junk_test: all checks OK"
	rv=0
    else
	rv=$?
	echo "FAIL: expected return value 0, but got ${rv}" >&2
	rv=1
    fi
    ( exit "${rv}" )
}


run_all_tests() {
    # The initial "true" here is so that all the tests have the same
    # indentation, to make it easier to automatically sort the lines.
    true &&
	run_test bad_c6_ext_map_test &&
	run_test be_incorrect_line_start_test &&
	run_test be_short_line_test &&
	run_test eof_in_line_num_1_test &&
	run_test eof_in_line_num_2_test &&
	run_test eol_in_extension_token_test &&
	run_test eol_in_line_number_test &&
	run_test fastvar_test &&
	run_test incomplete_eof_1_test &&
	run_test incomplete_eof_2_test &&
	run_test le_short_line_test &&
	run_test missing_eof_test &&
	run_test premature_eof_1_test &&
	run_test premature_eof_2_test &&
	run_test premature_eof_3_test &&
	run_test premature_eof_4_test &&
	run_test premature_eof_5_test &&
	run_test premature_eof_6_test &&
	run_test premature_eof_7_test &&
	run_test premature_eof_8_test &&
	run_test z80_trailing_junk_test &&
	true
    # The last "true" there is to make it more convenient to sort the lines
    # in the section above (i.e. so that the final test has && at the end,
    # like all the others).
}

main() {
    ( run_all_tests )
    rv=$?
    cleanup
    exit $rv
}

main "$@"
