#! /bin/sh
# Tags: negative
set -u

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

if ! incomplete_image="$(mktemp --tmpdir=${TMPDIR} incomplete_XXXXXX.ssd.gz)"
then
    echo "Unable to create a temporary file" >&2
    exit 1
fi

cleanup()
{
    rm -f "${incomplete_image}"
}

(
# Generate an incomple gzipped file by taking a prefix
# of a valid cmpressed file.
if ! dd if="${TEST_DATA_DIR}/watford-sd-62-with-62-files.ssd.gz" \
     of="${incomplete_image}" \
     count=1 bs=1470
then
    echo "Failed to generate temporary file" >&2
    exit 1
fi

# Verify that the prefix is incomplete and invalid
if gunzip < "${incomplete_image}" >/dev/null 2>/dev/null
then
    echo "Temporary file was unexpectedly valid" >&2
    exit 1
fi

if "${DFS}" --file "${incomplete_image}" cat
then
    echo "FAILURE: didn't reject an incomplete compressed file." >&2
    exit 1
fi
)
rv=$?
cleanup
exit $rv
