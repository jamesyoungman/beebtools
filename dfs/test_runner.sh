#! /bin/sh

# Arguments
# $0: name of this script
# $1: the name of the Bourne shell
# $2: name of the test script to run
# $3: name of the dfs executable
# $4: name of the test data directory
BOURNE_SHELL="$1"
shift
TEST_SCRIPT="$1"
shift
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift
#
# The working directory is writeable.
( "${BOURNE_SHELL}" "${TEST_SCRIPT}" "${DFS}" "${TEST_DATA_DIR}" )
