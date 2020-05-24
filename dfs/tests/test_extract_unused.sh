#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

check_extract_unused() {
    input="$1"
    expected="$2"

    if ! d="$(mktemp -d)"; then
	echo "failed to create temporary directory" >&1
	exit 1
    fi

    cleanup() {
	chmod +w "${d}"
	rm -f "${d}"/*
	rmdir "${d}"
    }

    dfs() {
	echo Running: "${DFS}" --file "${input}" "$@" >&2
	"${DFS}" --file "${input}" "$@"
    }

    gunzip < "${TEST_DATA_DIR}/${input}.gz" > "${input}" || exit 1

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


check_extract_unused acorn-dfs-sd-40t.ssd || exit 1

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
