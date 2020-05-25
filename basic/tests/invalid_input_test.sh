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

premature_eof_test() {
    expect_error "premature end-of-file" "${BBCBASIC_TO_TEXT}" --dialect=SDL ${TEST_DATA_DIR}/invalid/SDL/end.missing-eof.bbc
}

le_short_line_test() {
    expect_error "line.*length.*short" "${BBCBASIC_TO_TEXT}" --dialect=SDL ${TEST_DATA_DIR}/invalid/Z80/shortline.bbc
}

eol_in_line_number_test() {
    expect_error "end-of-line in the middle of a line number" \
		 "${BBCBASIC_TO_TEXT}" --dialect=6502 \
		 "${TEST_DATA_DIR}/invalid/6502/goto-premature-eol.bbc"
}


run_all_tests() {
    true &&
	run_test premature_eof_test &&
	run_test le_short_line_test &&
	run_test eol_in_line_number_test
}

main() {
    ( run_all_tests )
    rv=$?
    cleanup
    exit $rv
}

main "$@"
