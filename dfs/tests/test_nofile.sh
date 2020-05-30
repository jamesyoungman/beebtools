#! /bin/sh
set -u

DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

if "${DFS}" --file does_not_exist.ssd cat
then
    echo 'expected non-zero return value for nonexistent file, got zero' >&2
    exit 1
else
    exit 0
fi
