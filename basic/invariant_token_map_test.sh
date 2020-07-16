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
#
# Args: 1: bbcbasic_to_text executable
#       2: golden output file

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

set -e
if ! tf="$(mktemp  --tmpdir=${TMPDIR} invariant_token_map_test_XXXXXX)"
then
    echo "Failed to create a temporary file" >&2
    exit 1
fi

cleanup() {
    rm -f "${tf}"
}

if ! "$1" -D "${tf}"
then
    echo "Failed to write the current token map" >&2
    cleanup
    exit 1
fi
echo "Golden token file is $2"
echo "Current token file is ${tf}"
if ! diff -u "$2" "${tf}"
then
    echo "current token map does not match golden token map" >&2
    cleanup
    exit 1
fi

cleanup
exit 0
