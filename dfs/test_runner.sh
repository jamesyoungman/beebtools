#! /bin/sh

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
#
# The working directory is writeable.
(
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
exit $rv
