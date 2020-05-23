#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

(
  set -e

  "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-ss-80t-textfiles.ssd.gz" list 'LINES' >actual_0.txt
  diff -u "${TEST_DATA_DIR}/lines-list.txt" actual_0.txt

  "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-ss-80t-textfiles.ssd.gz" list '$.LINES' >actual_1.txt
  diff -u "${TEST_DATA_DIR}/lines-list.txt" actual_1.txt

  "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-ss-80t-textfiles.ssd.gz" list ':0.$.LINES' >actual_2.txt
  diff -u "${TEST_DATA_DIR}/lines-list.txt" actual_2.txt
)
rv=$?
rm -f actual_0.txt actual_1.txt actual_2.txt
exit $rv
