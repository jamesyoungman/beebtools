#! /bin/sh
set -u
set -e
# Tags: positive
# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

if ! actual="$(mktemp --tmpdir=${TMPDIR} test_sector_map_XXXXXX)"
then
    echo "Cannot create temporary file" >&2
    exit 1
fi

cleanup() {
    rm -f "${actual}"
}
trap cleanup EXIT

get_config() {
    # One subtlety: the storage configuration is printed on stderr.
    "${DFS}" "$@" 2>"${actual}" >&2
}

check_config_substring() {
    label="$1"; shift
    # all remaining arguments are literal strings
    #
    # Returned value is zero if the command succeeded and all strings matched.
    for substring
    do
	if ! grep -F -e "${substring}" < "${actual}" >/dev/null
	then
	    echo "${label}: output did not contain literal string ${substring}" >&2
	    echo "Output was:" >&2
	    cat "${actual}" >&2
	    return 1
	fi
    done
    echo "${label}: PASS"
    return 0
}

# Test the --drive-any option.
image="${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz"
if ! get_config --drive-first --show-config \
     --file "${image}" --file "${image}" --file "${image}" --file "${image}" \
     info ':0.no-file'
then
    echo "Cannot use all 4 drives for single-sided images" >&2
    exit 1
fi
check_config_substring T010 \
     "Drive 0: occupied, single density, 1 side, 40 tracks, 10 sectors per track, compressed SSD file ${image}" \
     "Drive 1: occupied, single density, 1 side, 40 tracks, 10 sectors per track, compressed SSD file ${image}" \
     "Drive 2: occupied, single density, 1 side, 40 tracks, 10 sectors per track, compressed SSD file ${image}" \
     "Drive 3: occupied, single density, 1 side, 40 tracks, 10 sectors per track, compressed SSD file ${image}" || exit 1

# Test the --drive-physical option.
if ! get_config --drive-physical --show-config \
     --file "${image}" --file "${image}" --file "${image}" --file "${image}" --file "${image}" \
     info ':0.no-file'
then
    echo "Cannot use 5 single-sided images" >&2
    exit 1
fi

# When inserting 5 single-sided disks, the first two go in the first
# two physical disc drives (as drives 0 and 1).  Drives 2 and 3 are
# the drives corresponding to side 1 of the discs in drives 0 and 1
# respectively, so they cannot be used in the --drive-physical
# allocation method.  Hence the next free drives are 4 and 5.
# Drives 6 and 7 are the side 1 drives for those, so the next
# free drive is 8.
#
desc="single density, 1 side, 40 tracks, 10 sectors per track, compressed SSD file ${image}"
check_config_substring T020 \
     "Drive 0: occupied, ${desc}" \
     "Drive 1: occupied, ${desc}" \
     "Drive 2: empty" \
     "Drive 3: empty" \
     "Drive 4: occupied, ${desc}" \
     "Drive 5: occupied, ${desc}" \
     "Drive 6: empty" \
     "Drive 7: empty" \
     "Drive 8: occupied, ${desc}" || exit 1
