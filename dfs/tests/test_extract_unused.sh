#! /bin/sh
# Tags: positive negative
# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

check_extract_unused() {
    input="$1"
    expected="$2"

    if ! d="$(mktemp --tmpdir=${TMPDIR} -d)"; then
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
	rm -f "${d}"/*
	chmod a-w "${d}"
	if dfs extract-unused "${d}"; then
	    echo "extract-unused command did not diagnose unwritable output directory" >&2
	    ls -l "${d}" >&2
	    exit 1
	fi

	# Invalid: request drive 1, no media in drive 1
	chmod a+w "${d}"
	rm -f "${d}"/*
	if dfs extract-unused --drive 1 "${d}"
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
if "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" extract-unused
then
    echo "FAIL: extract-unused command doesn't diagnose missing output directory" >&2
    exit 1
fi

# Invalid: spurious extra arguments
if "${DFS}" --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" extract-unused outdir too-many-args
then
    echo "FAIL: extract-unused command doesn't diagnose spurious arguments" >&2
    exit 1
fi
