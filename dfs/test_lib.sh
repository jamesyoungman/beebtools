#! /bin/sh
# This file is sourced in the same shell as each test runs in.
# Functions defined in this file are visible in the tests.

fails() {
    # Run "$@" and return status:
    # 0 if the command exited normally with an unsuccessful status code (1 to 128 inclusive)
    # 1 if the command exited with a fatal signal
    # 1 if the command exited with status 0
    # In both the cases where the return value is non-zero, an error message is also printed.
    "$@"
    fails_rv=$?
    if [ $fails_rv -eq 0 ]
    then
	echo 'Expected' "$@" 'to fail, but it did not' >&2
	return 1
    fi
    if [ $fails_rv -le 128 ]
    then
	# The command failed normally, good.
	return 0
    fi

    echo "$@" 'exited with unexpected status' $rv 'which is not a normal status code' >&2
    return 1
}

true
