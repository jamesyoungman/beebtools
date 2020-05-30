#! /bin/sh
set -u

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

if ! actual="$(mktemp --tmpdir=${TMPDIR} actual_list.XXXXXX.txt)"
then
    echo "failed to create a temporary file." >&2
    exit 1
fi

(
  set -e

  "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-ss-80t-textfiles.ssd.gz" list 'LINES' >"${actual}"
  diff -u "${TEST_DATA_DIR}/lines-list.txt" "${actual}"

  "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-ss-80t-textfiles.ssd.gz" list '$.LINES' >"${actual}"
  diff -u "${TEST_DATA_DIR}/lines-list.txt" "${actual}"

  "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-ss-80t-textfiles.ssd.gz" list ':0.$.LINES' >"${actual}"
  diff -u "${TEST_DATA_DIR}/lines-list.txt" "${actual}"
)
rv=$?
rm -f "${actual}"
exit $rv
