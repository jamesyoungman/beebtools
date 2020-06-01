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
    # all arguments are literal strings
    #
    # Returned value is zero if the command succeeded and all strings matched.
    for substring
    do
	if ! grep -F -e "${substring}" < "${actual}" >/dev/null
	then
	    echo "Output did not contain literal string ${substring}" >&2
	    echo "Output was:" >&2
	    cat "${actual}" >&2
	    return 1
	fi
    done
    return 0
}

# Test the --drive-any option.
image="${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz"
if ! get_config --drive-any --show-config \
     --file "${image}" --file "${image}" --file "${image}" --file "${image}" \
     info ':0.no-file'
then
    echo "Cannot use all 4 drives for single-sided images" >&2
    exit 1
fi
check_config_substring \
     "Drive 0: occupied, compressed image file ${image}" \
     "Drive 1: occupied, compressed image file ${image}" \
     "Drive 2: occupied, compressed image file ${image}" \
     "Drive 3: occupied, compressed image file ${image}" || exit 1

# Test the --drive-even option.
if ! get_config --drive-even --show-config \
     --file "${image}" --file "${image}" --file "${image}" --file "${image}" --file "${image}" \
     info ':0.no-file'
then
    echo "Cannot use 5 single-sided images" >&2
    exit 1
fi
check_config_substring \
     "Drive 0: occupied, compressed image file ${image}" \
     "Drive 2: occupied, compressed image file ${image}" \
     "Drive 4: occupied, compressed image file ${image}" \
     "Drive 6: occupied, compressed image file ${image}" \
     "Drive 8: occupied, compressed image file ${image}" || exit 1
