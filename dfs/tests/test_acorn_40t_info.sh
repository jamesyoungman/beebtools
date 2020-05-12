#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

input='acorn-dfs-sd-40t.ssd'

cleanup() {
    rm -f "${input}" last.command got.txt expected.txt
}

dfs() {
    echo "${DFS}" --file "${input}" "$@" > last.command
    "${DFS}" --file "${input}" "$@"
}

gunzip < "${TEST_DATA_DIR}/${input}.gz" > "${input}"
printf  >expected.txt '%s' \
'$.!BOOT       000000 FFFFFF 00000A 005
$.INIT     L  FF1900 FF8023 000012 004
B.NOTHING     FF1900 FF8023 000064 003
$.NOTHING     FF1900 FF8023 000064 002
'
dfs info '#.*' > got.txt
diff -u expected.txt got.txt
rv=$?
cleanup
( exit $rv )
