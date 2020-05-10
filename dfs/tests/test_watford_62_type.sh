#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

input='watford-sd-62-with-62-files.ssd'
init() {
    gunzip < "${TEST_DATA_DIR}/watford-sd-62-with-62-files.ssd.gz" > "${input}"
}
cleanup() {
    rm -f "${input}" last.command
}

dfs() {
    echo "${DFS}" --file "${input}" "$@" > last.command
    "${DFS}" --file "${input}" "$@"
    
}

expect_got() {
    label="$1"
    shift
    if test "$1" != "$2"
    then
	printf 'test %s: expected:\n%s\ngot:\n%s\n' "${label}" "$1" "$2"
	( printf "last dfs command was: " ; cat last.command ) >&2
	exit 1
    fi
}

(
    init; expect_got type_50  "$(printf 'FIFTY\n')" "$(dfs type          'FILE50')"
    init; expect_got type_50b "$(printf 'FIFTY\r')" "$(dfs type --binary 'FILE50')"
    init; expect_got dir      "$(printf 'FIFTY\n')" "$(dfs type          '$.FILE50')"
    init; expect_got drive    "$(printf 'FIFTY\n')" "$(dfs type          ':0.$.FILE50')"
)
rv=$?
cleanup
( exit $rv )


