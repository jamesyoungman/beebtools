#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift


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


# We have to decompress the image here, to verify that --show-config
# can distinguished compressed and uncompressed image files.
gunzip < "${TEST_DATA_DIR}"/acorn-dfs-sd-40t.ssd.gz > acorn-dfs-sd-40t.ssd
"${DFS}" --show-config --file 'acorn-dfs-sd-40t.ssd' info ':0.no-file' > actual.txt 2>&1 || (
	 rm -f acorn-dfs-sd-40t.ssd
	 exit 1
) || exit 1
rm -f acorn-dfs-sd-40t.ssd

check_config actual.txt \
	     '^Drive 0: occupied, image file acorn-dfs-sd-40t[.]ssd' \
	     '^Drive 1: empty' \
	     '^Drive 2: empty' \
	     '^Drive 3: empty' || exit 1

cp "${TEST_DATA_DIR}"/acorn-dfs-sd-40t.ssd.gz acorn-dfs-sd-40t.ssd.gz
"${DFS}" --show-config --file "acorn-dfs-sd-40t.ssd.gz" info ':0.no-file' > actual.txt 2>&1 || (
    rm -f acorn-dfs-sd-40t.ssd.gz
    exit 1
) || exit 1
rm -f acorn-dfs-sd-40t.ssd.gz

check_config actual.txt \
	     '^Drive 0: occupied, compressed image file acorn-dfs-sd-40t[.]ssd[.]gz' \
	     '^Drive 1: empty' \
	     '^Drive 2: empty' \
	     '^Drive 3: empty' || exit 1
