#! /bin/sh
# Tags: positive
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
if ! gunzip < "${TEST_DATA_DIR}"/acorn-dfs-sd-40t.ssd.gz >"${imagefile}"
then
    rm -f "${imagefile}"
    exit 1
fi

if ! "${DFS}" --show-config --file "${imagefile}" info ':0.nofile1' > actual.txt 2>&1
then
    rm -f "${imagefile}"
    cleanup
    exit 1
fi

rm -f "${imagefile}"
if ! check_config actual.txt \
	     '^Drive 0: occupied, SSD file .*show_config_imagefile_acorn-dfs-sd-40t[.]......[.]ssd' \
	     '^Drive 1: empty' \
	     '^Drive 2: empty' \
	     '^Drive 3: empty'
then
    cleanup
    exit 1
fi

if ! "${DFS}" --show-config --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" info ':0.nofile2' > actual.txt 2>&1
then
    cat actual.txt >&2
    cleanup
    exit 1
fi

if ! check_config actual.txt \
	     '^Drive 0: occupied, compressed image file .*/acorn-dfs-sd-40t[.]ssd[.]gz' \
	     '^Drive 1: empty' \
	     '^Drive 2: empty' \
	     '^Drive 3: empty'
then
    cleanup
    exit 1
fi

cleanup
exit 0
