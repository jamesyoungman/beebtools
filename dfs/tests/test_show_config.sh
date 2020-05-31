#! /bin/sh
# Tags: negative positive
set -u

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

cleanup() {
    rm -f actual.txt
}

check_config() {
    capture="$1"
    shift
    (
	for regex
	do
	    if ! egrep -q -e "${regex}" < "${capture}"
	    then
		echo "FAILED: expected output to match ${regex}, but it did not." >&2
		echo "Actual output was:" >&2
		cat actual.txt >&2
		exit 1
	    fi
	done
    )
    rv=$?
    cleanup
    ( exit $rv )
}


if ! imagefile="$(mktemp --tmpdir=${TMPDIR} show_config_imagefile_acorn-dfs-sd-40t.XXXXXX.ssd)"
then
    echo "Unable to create temporary file." >&2
    exit 1
fi

# We have to decompress the image here, to verify that --show-config
# can distinguish compressed and uncompressed image files.
gunzip < "${TEST_DATA_DIR}"/acorn-dfs-sd-40t.ssd.gz >"${imagefile}"
"${DFS}" --show-config --file "${imagefile}" info ':0.no-file' > actual.txt 2>&1 || (
	 rm -f "${imagefile}"
	 exit 1
) || exit 1

rm -f "${imagefile}"
check_config actual.txt \
	     '^Drive 0: occupied, image file .*show_config_imagefile_acorn-dfs-sd-40t[.]......[.]ssd' \
	     '^Drive 1: empty' \
	     '^Drive 2: empty' \
	     '^Drive 3: empty' || exit 1


"${DFS}" --show-config --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" info ':0.no-file' > actual.txt 2>&1 || (
    exit 1
) || exit 1

check_config actual.txt \
	     '^Drive 0: occupied, compressed image file .*/acorn-dfs-sd-40t[.]ssd[.]gz' \
	     '^Drive 1: empty' \
	     '^Drive 2: empty' \
	     '^Drive 3: empty' || exit 1
