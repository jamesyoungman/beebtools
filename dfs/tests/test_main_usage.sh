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
# Tags: negative
# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift


# usage error: missing command
if ! fails ${DFS} --file ${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz
then
    echo "${DFS} failed to diagnose a missing command." >&2
    exit 1
fi

# usage error: unknown command
if ! fails ${DFS} --file ${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz unknown-command
then
    echo "${DFS} failed to diagnose an unknown command." >&2
    exit 1
fi

# usage error: directory names must be one character
if ! fails ${DFS} --file ${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz --dir '$$' cat
then
    echo "${DFS} failed to reject a multi-character directory name" >&2
    exit 1
fi

# usage error: unknown option letter -Q
if ! fails  ${DFS} --file ${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz -Q cat
then
    echo "${DFS} failed to reject an unknown option letter" >&2
    exit 1
fi
