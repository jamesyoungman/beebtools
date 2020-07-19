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

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# TODO: move this into the test driver.
# Ensure TMPDIR is set.
: ${TMPDIR:?}

check_free() {
    input="$1"
    expected="$2"

    if ! expected_f="$(mktemp --tmpdir=${TMPDIR:?} test_free_expected.XXXXXX.txt)"
    then
	echo "failed to create a temporary file" >&2
	exit 1
    fi
    if ! actual_f="$(mktemp --tmpdir=${TMPDIR:?} test_free_actual.XXXXXX.txt)"
    then
	echo "failed to create a temporary file" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${expected_f}" "${actual_f}"
    }

    dfs() {
	"${DFS}" --file "${TEST_DATA_DIR}/${input}.gz" "$@"
    }

    (
	dfs free > "${actual_f}"
	printf '%s' "${expected}" > "${expected_f}"
	if ! diff -u "${expected_f}" "${actual_f}"
	then
	    printf 'Result of free command for %s is incorrect.\n' "${input}" >&2
	    hexdump -C "${expected_f}"
	    hexdump -C "${actual_f}"
	    exit 1
	fi
    )
    rv=$?
    cleanup
    ( exit $rv )
}


check_free acorn-dfs-sd-40t.ssd \
"27 Files 18A Sectors 100,864 Bytes Free
04 Files 006 Sectors   1,536 Bytes Used
"

# Watford DDFS doesn't have a FREE command
# but it has a SPACES command with a different
# output format (from which we can get the
# data to construct an expected-output which
# we would have obtained had Watford DDFS
# included *FREE).
check_free watford-sd-62-with-62-files.ssd \
"00 Files 2DE Sectors 187,904 Bytes Free
62 Files 042 Sectors  16,896 Bytes Used
"

echo Negative test: too many arguments.
if ! fails ${DFS} --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" free 0 0 0
then
	echo "${DFS} failed to diagnose spurious extra arguments" >&2
	exit 1
fi

echo Negative test: invalid drive number
if ! fails ${DFS} --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" free A
then
	echo "${DFS} failed to diagnose an invalid drive number" >&2
	exit 1
fi

echo Negative test: empty drive number
if ! fails ${DFS} --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" free ""
then
	echo "${DFS} failed to diagnose an empty drive number" >&2
	exit 1
fi

echo Negative test: no media in specified drive
if ! fails ${DFS} --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" free 1
then
	echo "${DFS} failed to diagnose missing media" >&2
	exit 1
fi

echo Negative test: missing media
if ! fails ${DFS} --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" --drive 1 free
then
	echo "${DFS} failed to diagnose missing media" >&2
	exit 1
fi
