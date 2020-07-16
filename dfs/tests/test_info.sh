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
