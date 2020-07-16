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
