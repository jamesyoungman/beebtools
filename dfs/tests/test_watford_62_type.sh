#! /bin/sh
set -u
# Tags: positive negative
# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

input='watford-sd-62-with-62-files.ssd.gz'

dfs() {
    "${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@"
}

expect_got() {
    label="$1"
    shift
    if test "$1" != "$2"
    then
	printf 'test %s: expected:\n%s\ngot:\n%s\n' "${label}" "$1" "$2"
	exit 1
    fi
}

(
    expect_got type_50   "$(printf 'FIFTY\n')" "$(dfs type          'FILE50'           || echo __FAILED__)"
    expect_got type_50b  "$(printf 'FIFTY\r')" "$(dfs type --binary 'FILE50'           || echo __FAILED__)"
    expect_got type_50bb "$(printf 'FIFTY\r')" "$(dfs type --binary --binary 'FILE50'  || echo __FAILED__)"
    expect_got dir       "$(printf 'FIFTY\n')" "$(dfs type          '$.FILE50'         || echo __FAILED__)"
    expect_got dirdash   "$(printf 'FIFTY\n')" "$(dfs type   --     '$.FILE50'         || echo __FAILED__)"
    expect_got drive     "$(printf 'FIFTY\n')" "$(dfs type          ':0.$.FILE50'      || echo __FAILED__)"

    # Use type on an uncompressed image file so that we exercise a call to
    # ImageFile::get_total_sectors().
    if ! temp_image="$(mktemp --tmpdir=${TMPDIR} tmp_unc_img_watford-sd-62-with-62-files.XXXXXX.ssd)"
    then
	echo "failed to create temporary file" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${temp_image}"
    }

    if ! gunzip < "${TEST_DATA_DIR}/watford-sd-62-with-62-files.ssd.gz" >"${temp_image}"
    then
	cleanup
	exit 1
    fi
    if ! (expect_got type_50   "$(printf 'FIFTY\n')" \
       "$(${DFS} --file ${temp_image} type 'FILE50' || echo __FAILED__ )" )
    then
	cleanup
	exit 1
    fi
    cleanup

    # Multiple arguments are not currently supported but they might
    # be in the future, so there is no test case about that.

    # Some usage errors and similar.
    echo "Bad-option test:"
    if ! fails dfs type  --not-an-option 'FILE50'
    then
	echo "FAIL: type command does not reject non-options" >&2
	exit 1
    fi

    echo "Empty argument test:"
    if ! fails dfs type ''
    then
	echo "FAIL: type command does not reject empty arguments" >&2
	exit 1
    fi
)
