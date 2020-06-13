#! /bin/sh
# Tags: positive negative
set -u

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

check_sector_map() {

    if ! actual="$(mktemp --tmpdir=${TMPDIR} test_sector_map_XXXXXX)"
    then
	echo "Cannot create temporary file" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${actual}"
    }

    input="${TEST_DATA_DIR}/watford-sd-62-with-holes.ssd.gz"
    golden="${TEST_DATA_DIR}/watford-sd-62-with-holes.ssd.sectors"
    (
	if fails "${DFS}" --file "${input}"  sector-map "$@" >"${actual}"
	then
	    echo "sector-map exited with wrong status" >&2
	    exit 1
	else
	    if diff -b "${golden}" "${actual}"
	    then
		return
	    else
		echo "FAIL sector map output for $@ differs from golden file ${golden}" >&2
		exit 1
	    fi
	fi
    )
    rv=$?
    cleanup
    ( exit $rv )
}

check_sector_map && check_sector_map 0 || exit 1

if ! fails "${DFS}" --file does-not-exist.ssd  sector-map
then
	echo "sector-map succeeded on nonexistent disc image file" >&2
	exit 1
fi
