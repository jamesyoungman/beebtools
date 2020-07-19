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
# Tags: positive negative
set -u

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:?}

check_sector_map() {

    if ! actual="$(mktemp --tmpdir=${TMPDIR:?} test_sector_map_XXXXXX)"
    then
	echo "Cannot create temporary file" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${actual}"
    }

    input="${TEST_DATA_DIR}/watford-sd-62-with-holes.ssd.gz"
    golden="${TEST_DATA_DIR}/watford-sd-62-with-holes.ssd.sectors"
    (
	if fails "${DFS}" --file "${input}"  sector-map "$@" >"${actual}"
	then
	    echo "sector-map exited with wrong status" >&2
	    exit 1
	else
	    if diff -b "${golden}" "${actual}"
	    then
		return
	    else
		echo "FAIL sector map output for $@ differs from golden file ${golden}" >&2
		exit 1
	    fi
	fi
    )
    rv=$?
    cleanup
    ( exit $rv )
}

check_sector_map && check_sector_map 0 || exit 1

if ! fails "${DFS}" --file does-not-exist.ssd  sector-map
then
	echo "sector-map succeeded on nonexistent disc image file" >&2
	exit 1
fi
