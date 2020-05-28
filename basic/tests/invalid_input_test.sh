#! /bin/sh

# ARGS:
# bbcbasic_to_text
# test data directory
BBCBASIC_TO_TEXT="$1"
TEST_DATA_DIR="$2"

cleanup() {
    rm -f stderr.output last.command
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

incomplete_eof_1_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=Z80 "${TEST_DATA_DIR}"/invalid/Z80/incomplete-eof-marker-1.bbc
}

incomplete_eof_2_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=Z80 "${TEST_DATA_DIR}"/invalid/Z80/incomplete-eof-marker-2.bbc
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

run_all_tests() {
    true &&
	run_test missing_eof_test &&
	run_test incomplete_eof_1_test &&
	run_test incomplete_eof_2_test &&
	run_test premature_eof_1_test &&
	run_test premature_eof_2_test &&
	run_test le_short_line_test &&
	run_test eol_in_line_number_test &&
	run_test bad_c6_ext_map_test &&
	run_test fastvar_test
}

main() {
    ( run_all_tests )
    rv=$?
    cleanup
    exit $rv
}

main "$@"
