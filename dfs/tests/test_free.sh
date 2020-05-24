#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift


check_free() {
    input="$1"
    expected="$2"

    cleanup() {
	rm -f "${input}" expected.txt actual.txt
    }

    dfs() {
	"${DFS}" --file "${input}" "$@"
    }

    gunzip < "${TEST_DATA_DIR}/${input}.gz" > "${input}" || exit 1

    (
	dfs free > actual.txt
	printf '%s' "${expected}" > expected.txt
	if ! diff -u expected.txt actual.txt
	then
	    printf 'Result of free command for %s is incorrect.\n' "${input}" >&2
	    hexdump -C expected.txt
	    hexdump -C actual.txt
	    exit 1
	fi
    )
    rv=$?
    cleanup
    ( exit $rv )
}


check_free acorn-dfs-sd-40t.ssd \
"27 Files 18A Sectors 100,864 Bytes Free
04 Files 006 Sectors   1,536 Bytes Used
"

# Watford DDFS doesn't have a FREE command
# but it has a SPACES command with a different
# output format (from which we can get the
# data to construct an expected-output which
# we would have obtained had Watford DDFS
# included *FREE).
check_free watford-sd-62-with-62-files.ssd \
"00 Files 2DE Sectors 187,904 Bytes Free
62 Files 042 Sectors  16,896 Bytes Used
"

# Negative test: too many arguments.
if ${DFS} --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" free 0 0 0
then
	echo "${DFS} failed to diagnose spurious extra arguments" >&2
	exit 1
fi

# Negative test: invalid drive number
if ${DFS} --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" free A
then
	echo "${DFS} failed to diagnose an invalid drive number" >&2
	exit 1
fi

# Negative test: no media in specified drive
if ${DFS} --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" free 1
then
	echo "${DFS} failed to diagnose missing media" >&2
	exit 1
fi
if ${DFS} --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" --drive 1 free
then
	echo "${DFS} failed to diagnose missing media" >&2
	exit 1
fi
