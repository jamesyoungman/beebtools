#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift


check_space() {
    input="$1"
    expected="$2"

    cleanup() {
	rm -f "${input}" expected.txt actual.txt
    }

    dfs() {
	echo Running: "${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@" >&2
	"${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@"
    }

    (
	dfs space > actual.txt || exit 1
	printf '%s' "${expected}" > expected.txt
	if ! diff -u expected.txt actual.txt
	then
	    printf 'Result of free command for %s is incorrect.\n' "${input}" >&2
	    echo "Expected output:"
	    hexdump -C expected.txt
	    echo "Actual output:"
	    hexdump -C actual.txt
	    exit 1
	fi
    )
    rv=$?
    cleanup
    ( exit $rv )
}


check_space acorn-dfs-sd-40t.ssd.gz \
"Gap sizes on disc 0:
18A

Total space free = 18A sectors
"

check_space watford-sd-62-with-62-files.ssd.gz \
"Gap sizes on disc 0:
2DE

Total space free = 2DE sectors
"

check_space watford-sd-62-with-holes.ssd.gz \
"Gap sizes on disc 0:
001 002 009 002 002 2DE 001 002 002 00A

Total space free = 2FD sectors
"

## Check various usage errors.
input="${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz"
if ! [ -f "${input}" ]
then
    echo "Input file ${input} is missing" >&2
    exit 1
fi
# No media at all.
if "${DFS}" space
then
    echo "FAIL: spurious success status when no media is present" >&2
    exit 1
fi

# Default drive has no media.
if "${DFS}" --file "${input}" --drive 1 space
then
    echo "FAIL: spurious success status when default drive has no media" >&2
    exit 1
fi

# Explicitly specified drive has no media.
if "${DFS}" --file "${input}" space 1
then
    echo "FAIL: spurious success status when specified drive has no media" >&2
    exit 1
fi

# Explicitly specified drive is not a valid drive number
if "${DFS}" --file "${input}" space X
then
    echo "FAIL: spurious success status when specified drive is not a number" >&2
    exit 1
fi
if "${DFS}" --file "${input}" space 4
then
    echo "FAIL: spurious success status when specified drive is not a valid drive number" >&2
    exit 1
fi

# Spurious argument
if "${DFS}" --file "${input}" space 0 0
then
    echo "FAIL: spurious success status with extra argument" >&2
    exit 1
fi
