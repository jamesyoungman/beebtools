#! /bin/sh
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
if ! diff "$2" "${tf}"
then
    echo "current token map does not match golden token map" >&2
    cleanup
    exit 1
fi

cleanup
exit 0
