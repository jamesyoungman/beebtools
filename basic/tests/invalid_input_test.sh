#! /bin/sh

# ARGS:
# bbcbasic_to_text
# test data directory
BBCBASIC_TO_TEXT="$1"
TEST_DATA_DIR="$2"

cleanup() {
    rm -f stdout.output stderr.output expected.output last.command
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
    echo "$@" > last.command
    if "$@" 2>stderr.output
    then
	error "$@ should have exited with a nonzero status but it didn't"
	cat stderr.output >&2
	return 1
    fi
    if egrep -r "${regex}" < stderr.output >/dev/null
    then
	return 0
    fi
    error "$@ should have issued an error message matching ${regex} but it didn't"
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
    expect_error "line.*length.*short" "${BBCBASIC_TO_TEXT}" --dialect=SDL ${TEST_DATA_DIR}/invalid/Z80/shortline.bbc
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

fastvar_test() {
    expect_error "crunched.*this tool on the original source code" \
		 "${BBCBASIC_TO_TEXT}" --dialect=SDL \
		 "${TEST_DATA_DIR}/invalid/SDL/sdl-fastvar.bbc"
}

z80_trailing_junk_test() {
    # This test is unusual since the input is in some sense incorrect
    # we (we think) we see this kind of input in the wild, with other
    # characters in the file after the logical EOF marker.
    rv=4  # 4 signals that we forgot to set a proper value.
    if "${BBCBASIC_TO_TEXT}" \
	   --dialect=Z80 \
	   "${TEST_DATA_DIR}"/inputs/Z80/trailing-junk.bbc \
	   >stdout.output 2>stderr.output
    then
	printf '   10 CLS\n' > expected.output
	if ! diff -u expected.output stdout.output >&2
	then
	    echo "FAIL: incorrect output on stdout" >&2
	    rv=1
	fi
	wanted='expected end-of-file'
	if ! grep -F -q "${wanted}" stderr.output
	then
	    echo "FAIL: expected to see ${wanted} in the stderr output" >&2
	    echo "instead, got this:" >&2
	    cat < stderr.output >&2
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
    true &&
	run_test missing_eof_test &&
	run_test incomplete_eof_1_test &&
	run_test incomplete_eof_2_test &&
	run_test premature_eof_1_test &&
	run_test premature_eof_2_test &&
	run_test premature_eof_3_test &&
	run_test eof_in_line_num_1_test &&
	run_test eof_in_line_num_2_test &&
	run_test le_short_line_test &&
	run_test eol_in_line_number_test &&
	run_test bad_c6_ext_map_test &&
	run_test fastvar_test &&
	run_test z80_trailing_junk_test
}

main() {
    ( run_all_tests )
    rv=$?
    cleanup
    exit $rv
}

main "$@"
