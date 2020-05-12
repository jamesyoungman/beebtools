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
	echo Running: "${DFS}" --file "${input}" "$@" >&2
	"${DFS}" --file "${input}" "$@"
    }

    gunzip < "${TEST_DATA_DIR}/${input}.gz" > "${input}" || exit 1

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


check_space acorn-dfs-sd-40t.ssd \
"Gap sizes on disc 0:
18A

Total space free = 18A sectors
"

check_space watford-sd-62-with-62-files.ssd \
"Gap sizes on disc 0:
2DE

Total space free = 2DE sectors
"

check_space watford-sd-62-with-holes.ssd \
"Gap sizes on disc 0:
001 002 009 002 002 2DE 001 002 002 00A

Total space free = 2FD sectors
"

