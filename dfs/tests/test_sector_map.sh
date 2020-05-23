#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

check_sector_map() {

    cleanup() {
	rm -f actual.txt
    }

    input="${TEST_DATA_DIR}/watford-sd-62-with-holes.ssd.gz"
    golden="${TEST_DATA_DIR}/watford-sd-62-with-holes.ssd.sectors"
    (
	"${DFS}" --file "${input}"  sector-map "$@" > actual.txt
	if diff -b "${golden}" actual.txt
	then
	    return
	else
	    echo "FAIL sector map output for $@ differs from golden file ${golden}" >&2
	    exit 1
	fi
    )
    rv=$?
    cleanup
    ( exit $rv )
}

check_sector_map && check_sector_map 0 || exit 1

if "${DFS}" --file does-not-exist.ssd  sector-map
then
	echo "sector-map succeeded on nonexistent disc image file" >&2
	exit 1
fi
