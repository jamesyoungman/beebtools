#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

fsp=':A.$.LINES'
if "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-ss-80t-textfiles.ssd.gz" list "${fsp}"
then
    echo "An invalid file specificatin ${fsp} was accepted" >&2
    exit 1
fi
