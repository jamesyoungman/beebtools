#! /bin/sh
# Tags: negative
# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift


if ! "${DFS}" \
     --show-config \
     --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" \
     --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" \
     --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" \
     --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" \
     info ':0.no-file'
then
    echo "Cannot use all 4 drives for single-sided images" >&2
    exit 1
fi

if "${DFS}" \
     --show-config \
     --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" \
     --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" \
     --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" \
     --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" \
     --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" \
     info ':0.no-file'
then
    echo "Apparently used more drives than DFS supports" >&2
    exit 1
fi
