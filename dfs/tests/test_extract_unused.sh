#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

check_extract_unused() {
    input="$1"
    expected="$2"

    if ! d="$(mktemp -d)"; then
	echo "failed to create temporary directory" >&1
	exit 1
    fi

    cleanup() {
	rm -f "${d}"/*
	rmdir "${d}"
    }

    dfs() {
	echo Running: "${DFS}" --file "${input}" "$@" >&2
	"${DFS}" --file "${input}" "$@"
    }

    gunzip < "${TEST_DATA_DIR}/${input}.gz" > "${input}" || exit 1

    (
	if ! dfs extract-unused "${d}"; then
	    echo "extract-unused command failed" >&2
	    exit 1
	fi
    )
    rv=$?
    cleanup
    ( exit $rv )
}


check_extract_unused acorn-dfs-sd-40t.ssd
