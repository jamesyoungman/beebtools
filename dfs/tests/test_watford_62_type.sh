#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

input='watford-sd-62-with-62-files.ssd.gz'

cleanup() {
    rm -f last.command
}

dfs() {
    echo "${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@" > last.command
    "${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@"
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
    expect_got type_50   "$(printf 'FIFTY\n')" "$(dfs type          'FILE50')"
    expect_got type_50b  "$(printf 'FIFTY\r')" "$(dfs type --binary 'FILE50')"
    expect_got type_50bb "$(printf 'FIFTY\r')" "$(dfs type --binary --binary 'FILE50')"
    expect_got dir       "$(printf 'FIFTY\n')" "$(dfs type          '$.FILE50')"
    expect_got dirdash   "$(printf 'FIFTY\n')" "$(dfs type   --     '$.FILE50')"
    expect_got drive     "$(printf 'FIFTY\n')" "$(dfs type          ':0.$.FILE50')"

    # Use type on an uncompressed image file so that we exercise a call to
    # ImageFile::get_total_sectors().
    gunzip \
	< "${TEST_DATA_DIR}/watford-sd-62-with-62-files.ssd.gz" \
	> 'watford-sd-62-with-62-files.ssd' || exit 1
    if ! (expect_got type_50   "$(printf 'FIFTY\n')" \
       "$(${DFS} --file watford-sd-62-with-62-files.ssd type 'FILE50')" )
    then
	rm -f 'watford-sd-62-with-62-files.ssd'
	exit 1
    fi
    rm -f 'watford-sd-62-with-62-files.ssd'

    # Some usage errors and similar.
    if dfs type  --not-an-option 'FILE50'
    then
	echo "FAIL: type command does not reject non-options" >&2
	exit 1
    fi
    if dfs type '' 'FILE50'
    then
	echo "FAIL: type command does not reject empty arguments" >&2
	exit 1
    fi
)
rv=$?
cleanup
( exit $rv )
