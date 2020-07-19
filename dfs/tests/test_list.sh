#! /bin/sh
#
#   Copyright 2020 James Youngman
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# Tags: positive

set -u

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:?}

if ! actual="$(mktemp --tmpdir=${TMPDIR:?} actual_list.XXXXXX.txt)"
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
