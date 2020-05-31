#! /bin/sh
# Tags: positive
set -u

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

expected="expected_stdout_acorn-dfs-sd-40t.txt"
got="got_stdout_acorn-dfs-sd-40t.txt"

# We uncompress here to ensure that there is at least one test which
# operates on an uncompressed input file.
base='acorn-dfs-sd-40t.ssd'
if ! input="$(mktemp --tmpdir=${TMPDIR} acorn-dfs-sd-40t.XXXXXX.ssd)"
then
    echo "Unable to create a temporary file" >&2
    exit 1
fi
gunzip < "${TEST_DATA_DIR}/${base}.gz" > "${input}" || exit 1

cleanup() {
    rm -f "${input}" "${got}" "${expected}"
}

dfs() {
    "${DFS}" --file "${input}" "$@"
}

printf  >"${expected}" '%s' \
'$.!BOOT       000000 FFFFFF 00000A 005
$.INIT     L  FF1900 FF8023 000012 004
B.NOTHING     FF1900 FF8023 000064 003
$.NOTHING     FF1900 FF8023 000064 002
'
rv=0

dfs info '#.*' > "${got}" || rv=1
diff -u "${expected}" "${got}" || rv=1

dfs info '*.*' > "${got}" || rv=1
diff -u "${expected}" "${got}" || rv=1


cleanup
( exit $rv )
