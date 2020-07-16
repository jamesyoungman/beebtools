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

bad_listo_test() {
    for opt in -1 8 fnord 6.5
    do
	should_fail "${BBCBASIC_TO_TEXT}" --listo="${opt}" "${TEST_DATA_DIR}/inputs/6502/PICTURE"
    done
}

unknown_option_test() {
    should_fail "${BBCBASIC_TO_TEXT}" -Q "${TEST_DATA_DIR}/inputs/6502/PICTURE"
}

unknown_dialect_test() {
    should_fail "${BBCBASIC_TO_TEXT}" --dialect=Rhubarb "${TEST_DATA_DIR}/inputs/6502/PICTURE"
}

missing_input_test() {
    should_fail "${BBCBASIC_TO_TEXT}" "${TEST_DATA_DIR}/inputs/6502/__MISSING__"
}

all_tests() {
    true &&
	run_test bad_listo_test &&
	run_test unknown_option_test &&
	run_test unknown_dialect_test &&
	run_test missing_input_test
}

main() {
    # ARGS: ${BBCBASIC_TO_TEXT} ${TEST_DATA_DIR}
    BBCBASIC_TO_TEXT="$1"
    TEST_DATA_DIR="$2"
    all_tests
}

main "$@"
