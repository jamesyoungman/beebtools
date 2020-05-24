#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# We uncompress here to ensure that there is at least one test which
# operates on an uncompressed input file.
input='acorn-dfs-sd-40t.ssd'
gunzip < "${TEST_DATA_DIR}/${input}.gz" > "${input}" || exit 1

cleanup() {
    rm -f "${input}" last.command got.txt expected.txt
}

dfs() {
    echo "${DFS}" --file "${input}" "$@" > last.command
    "${DFS}" --file "${input}" "$@"
}

printf  >expected.txt '%s' \
'$.!BOOT       000000 FFFFFF 00000A 005
$.INIT     L  FF1900 FF8023 000012 004
B.NOTHING     FF1900 FF8023 000064 003
$.NOTHING     FF1900 FF8023 000064 002
'
rv=0

dfs info '#.*' > got.txt || rv=1
diff -u expected.txt got.txt || rv=1

dfs info '*.*' > got.txt || rv=1
diff -u expected.txt got.txt || rv=1


cleanup
( exit $rv )
