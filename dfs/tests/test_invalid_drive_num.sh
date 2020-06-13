#! /bin/sh
# Tags: positive negative
# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

check_invalid_drive() {
    drivenum="$1"

    dfs() {
	echo Running: "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" --drive "${drivenum}" "$@" >&2
	"${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" --drive "${drivenum}" "$@" >&2
    }

    if ! fails dfs cat 0
    then
	echo "${DFS} accepted an invalid drive number '${drivenum}'" >&2
	exit 1
    fi
}

check_valid_drive() {
    drivenum="$1"

    dfs() {
	echo Running: "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" --drive "${drivenum}" "$@" >&2
	"${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" --drive "${drivenum}" "$@" >&2
    }

    if ! dfs cat 0
    then
	echo "${DFS} rejected a valid drive number '${drivenum}'" >&2
	exit 1
    fi
}

# We also accept a leading + mostly because the conversion library
# routies do and it would be tricky to reject that.  We may not
# continue to accept it.
check_valid_drive     0
check_valid_drive     1
check_valid_drive     2
check_valid_drive     3
# We accept drive numbers larger than 3.
check_valid_drive     4
check_valid_drive     200

check_invalid_drive   A
check_invalid_drive   -1
check_invalid_drive   2x
check_invalid_drive   -10000000000000000000000000000000
check_invalid_drive    10000000000000000000000000000000

# These cases are more arguable.
check_valid_drive     " 0"
check_valid_drive     " 1"
check_valid_drive     " 2"
check_valid_drive     " 3"
