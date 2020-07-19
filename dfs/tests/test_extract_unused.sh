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

# Ensure TMPDIR is set.
: ${TMPDIR:?}

check_extract_unused() {
    input="$1"
    expected="$2"

    if ! d="$(mktemp --tmpdir=${TMPDIR:?} -d)"; then
	echo "failed to create temporary directory" >&1
	exit 1
    fi

    cleanup() {
	chmod +w "${d}"
	rm -f "${d}"/*
	rmdir "${d}"
    }

    dfs() {
	echo Running: "${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@" >&2
	"${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@"
    }

    (
	# Positive case
	if ! dfs extract-unused "${d}"; then
	    echo "extract-unused command failed" >&2
	    exit 1
	fi

	# Negative case: unwritable output directory.
	if [ $(id -u) -eq 0 ]
	then
	    # A directory with no 'w' bit is not actually unwritable
	    # for root, so skip this test if we are root (as is the
	    # case in github actions).
	    echo "Skipping unwritable-directory test, since we're running as root."
	else
            rm -f "${d}"/*
            chmod a-w "${d}"
            if ! fails dfs extract-unused "${d}"; then
                echo "extract-unused command did not diagnose unwritable output directory" >&2
                ls -ld "${d}" >&2
                ls -l "${d}" >&2
                exit 1
            fi
	fi

	# Invalid: request drive 1, no media in drive 1
	chmod a+w "${d}"
	rm -f "${d}"/*
	if ! fails dfs extract-unused --drive 1 "${d}"
	then
	    echo "FAIL: extract-unused command doesn't diagnose missing media" >&2
	    ls -l "${d}" >&2
	    exit 1
	fi

    )
    rv=$?
    cleanup
    ( exit $rv )
}


# Sanity check for check_extract_unused to make sure it can
# diagnose a missing input filr.
if check_extract_unused this-file-does-not-exist.ssd
then
    echo "test bug: check_extract_unused accepts a missing image file" >&2
    exit 1
fi

check_extract_unused acorn-dfs-sd-40t.ssd.gz || exit 1

# Two test cases with a short disc image, one compressed the other
# not.  This disc image was created in B-EM using "Disc > New disc
# :0/2..."  but *FORM80 was not used.  The emulated BBC Micro is fine
# with this (for example *CAT works) so we should accept it, too.
check_extract_unused acorn-dfs-40t-single-density-single-sided-empty.ssd.gz || exit 1
check_extract_unused acorn-dfs-40t-single-density-single-sided-empty.ssd || exit 1

# Now check some usage errors.

# Invalid: no destination directory.
if ! fails "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" extract-unused
then
    echo "FAIL: extract-unused command doesn't diagnose missing output directory" >&2
    exit 1
fi

# Invalid: spurious extra arguments
if ! fails "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" extract-unused outdir too-many-args
then
    echo "FAIL: extract-unused command doesn't diagnose spurious arguments" >&2
    exit 1
fi
