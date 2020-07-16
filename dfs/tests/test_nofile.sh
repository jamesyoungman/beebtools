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
set -u
# Tags: negative
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

if ! fails "${DFS}" --file does_not_exist.ssd cat
then
    echo 'expected non-zero return value for nonexistent file, got zero' >&2
    exit 1
else
    exit 0
fi
