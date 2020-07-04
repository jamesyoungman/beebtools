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

rv=0

check_space() {
    input="$1"
    expected_stdout="$2"

    if ! expected="$(mktemp --tmpdir=${TMPDIR} expected_space_output_XXXXXX.txt)"
    then
	echo "failed to create temporary file" >&2
	exit 1
    fi
    if ! actual="$(mktemp --tmpdir=${TMPDIR} actual_space_output_XXXXXX.txt)"
    then
	echo "failed to create temporary file" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${expected}" "${actual}"
    }

    dfs() {
	echo Running: "${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@" >&2
	"${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@"
    }

    (
	dfs space >"${actual}" || exit 1
	cat "${actual}"
	printf '%s' "${expected_stdout}" >"${expected}"
	if ! diff -u "${expected}" "${actual}"
	then
	    printf 'Result of free command for %s is incorrect.\n' "${TEST_DATA_DIR}/${input}" >&2
	    echo "Expected output:"
	    hexdump -C "${expected}"
	    echo "Actual output:"
	    hexdump -C "${actual}"
	    exit 1
	fi
    )
    rv=$?
    cleanup
    ( exit $rv )
}

check_space acorn-dfs-sd-80t-empty.ssd \
"Gap sizes on disc 0:
31E

Total space free = 31E sectors
" || rv=1

check_space acorn-dfs-sd-40t.ssd.gz \
"Gap sizes on disc 0:
18A

Total space free = 18A sectors
" || rv=1

check_space watford-sd-62-with-62-files.ssd.gz \
"Gap sizes on disc 0:
2DE

Total space free = 2DE sectors
" || rv=1

check_space watford-sd-62-with-holes.ssd.gz \
"Gap sizes on disc 0:
001 002 009 002 002 2DE 001 002 002 00A

Total space free = 2FD sectors
" || rv=1

# This image is special because all of the catalogue entries for files
# are in the second catalogue fragment.
check_space watford-sd-62-first-cat-empty.ssd.gz \
"Gap sizes on disc 0:
001 002 009 002 002 2DE 01F

Total space free = 30D sectors
" || rv=1

## Check various usage errors.
input="${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz"
if ! [ -f "${input}" ]
then
    echo "Input file ${input} is missing" >&2
    rv=1
fi
# No media at all.
echo "test: no media"
if ! fails "${DFS}" space
then
    echo "FAIL: spurious success status when no media is present" >&2
    rv=1
fi

# Default drive has no media.
echo "test: no media in default drive"
if ! fails "${DFS}" --file "${input}" --drive 1 space
then
    echo "FAIL: spurious success status when default drive has no media" >&2
    rv=1
fi

# Explicitly specified drive has no media.
echo "test: no media in specified drive"
if ! fails "${DFS}" --file "${input}" space 1
then
    echo "FAIL: spurious success status when specified drive has no media" >&2
    rv=1
fi

# Explicitly specified drive is not a valid drive number
echo "test: specified drive is not a valid drive number"
if ! fails "${DFS}" --file "${input}" space X
then
    echo "FAIL: spurious success status when specified drive is not a number" >&2
    rv=1
fi

echo "test: specified drive is an out-of-range drive number"
if ! fails "${DFS}" --file "${input}" space 4
then
    echo "FAIL: spurious success status when specified drive is not a valid drive number" >&2
    rv=1
fi

# Additional arguments
if ! actualm="$(mktemp --tmpdir=${TMPDIR} actual_space_outputM_XXXXXX.txt)"
then
    echo "failed to create temporary file" >&2
    exit 1
fi
if ! expectedm="$(mktemp --tmpdir=${TMPDIR} expected_space_outputM_XXXXXX.txt)"
then
    echo "failed to create temporary file" >&2
    exit 1
fi
(
echo "test: extra args"
if ! "${DFS}" --file "${input}" space 0 0 >"${actualm}"
then
    echo "FAIL: space command fails with extra args" >&2
    exit 1
fi
output() {
    printf '%s' \
"Gap sizes on disc 0:
18A

Total space free = 18A sectors
"
}
{
    # We have a repeated arg "0" so we emit the space
    # for that "volume" twice and then a summary.
    output;
    output;
    printf '%s' \
"Total space free in volume    0 = 018A sectors
Total space free in all volumes = 018A sectors
"
} >"${expectedm}"
if ! diff -u "${expectedm}" "${actualm}"
then
    printf 'Result of space command for %s space 0 0 is incorrect.\n' "${input}" >&2
    exit 1
fi
echo "test: extra args: passed"
) || rv=1
rm -f "${actualm}" "${expectedm}"

exit "$rv"
