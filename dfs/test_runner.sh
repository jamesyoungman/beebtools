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

# Arguments
# $0: name of this script
# $1: the name of the Bourne shell
# $2: name of the test script to run
# $3: name of the dfs executable
# $4: name of the test library file
# $5: name of the test data directory
BOURNE_SHELL="$1"
shift
TEST_SCRIPT="$1"
shift
DFS="$1"
shift
TEST_LIB="$1"
shift
TEST_DATA_DIR="$1"
shift

: ${TMPDIR:=/tmp}

if ! child_tmp=$(mktemp -d "${TMPDIR}/beebtools_test_runner.XXXXXX")
then
    echo "Unable to create temporary directory" >&2
    exit 1
fi

#
# The working directory is writeable.
(
    TMPDIR=${child_tmp}
    export TMPDIR

    set x "${DFS}" "${TEST_DATA_DIR}"
    echo "sourcing ${TEST_LIB}..."
    if ! . "${TEST_LIB}"
    then
	echo "Unable to source ${TEST_LIB}" >&2
	exit 1
    fi
    # Instead of shelling out to a new Bourne
    # shell to run the test, use this subshell.
    # That way we can use library functions from
    # ${TEST_LIB}.
    echo "running test ${TEST_SCRIPT}..."
    shift
    BASH_ARGV0="${TEST_SCRIPT}"
    . "${TEST_SCRIPT}"
)
rv=$?
echo "test ${TEST_SCRIPT} returned status $rv"

junk="$(find ${child_tmp}/* -print)"
if [ -n "${junk}" ]
then
    printf 'Inadequate cleanup after test:\n%s\n'  "${junk}" >&2
    rv=1
fi
rm -rf "${child_tmp}"

exit $rv
