#! /bin/sh

# Preserve the original stdout and stderr
# so that we can issue error messages.
exec 5>&1
exec 6>&2

error() {
    echo "$@" >&6
}

fatal() {
    echo FATAL: "$@" >&6
    exit 1
}

should_fail() {
    if "$@"
    then
	error "Command $@ did not fail, but it was expected to"
	exit 1
    fi
}

format_program_stdout_is_full_test() {
    input="${TEST_DATA_DIR}/inputs/6502/PICTURE"
    if ! [ -f "${input}" ]
    then
	fatal "input file ${input} is missing"
    fi
    should_fail "${BBCBASIC_TO_TEXT}" --dialect=6502 --listo=0 "${input}" >"${FULL}"
}

dialect_help_stdout_is_full_test() {
    should_fail "${BBCBASIC_TO_TEXT}" --dialect help >"${FULL}"
}

dialect_help_stderr_is_full_test() {
    should_fail "${BBCBASIC_TO_TEXT}" --dialect help 2>"${FULL}"
}

help_stdout_is_full_test() {
    should_fail "${BBCBASIC_TO_TEXT}" --help >"${FULL}"
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

full_tests() {
    true &&
	run_test dialect_help_stdout_is_full_test &&
	run_test dialect_help_stderr_is_full_test &&
	run_test format_program_stdout_is_full_test &&
	run_test help_stdout_is_full_test
}

main() {
    # ARGS: ${BBCBASIC_TO_TEXT} ${TEST_DATA_DIR}
    BBCBASIC_TO_TEXT="$1"
    TEST_DATA_DIR="$2"
    FULL=''

    if [ -e /dev/full ]
    then
	FULL='/dev/full'
    else
	echo 'Cannot find /dev/full, skipping fullness tests' >&2
	exit 0
    fi
    full_tests || exit 1
}

main "$@"
