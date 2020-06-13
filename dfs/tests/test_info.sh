#! /bin/sh
# Tags: negative

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift


dfs() {
    echo "${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@" > last.command
    "${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@" > last.command
}

input='acorn-dfs-sd-40t.ssd.gz'
if ! fails dfs info
then
    echo "FAILED: info should require a wildcard argument" >&2
    exit 1
fi

input='acorn-dfs-sd-40t.ssd.gz'
if ! fails dfs info FIRST SECOND
then
    echo "FAILED: info should require just one wildcard argument" >&2
    exit 1
fi

if ! fails dfs info ':3.#.*'
then
    echo "FAILED: info accepted a drive number having no media" >&2
    exit 1
fi
