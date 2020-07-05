#! /bin/sh
set -u

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

check_cat_impl() {
    input="$1"
    ui="$2"
    extras="$3"
    expected_text="$4"
    shift 4

    if ! actual="$(mktemp --tmpdir=${TMPDIR} cat_out_XXXXXX)"
    then
	echo "Unable to create a temporary file" >&2
	exit 1
    fi

    if ! fexpected="$(mktemp --tmpdir=${TMPDIR} cat_exp_XXXXXX)"
    then
	echo "Unable to create a temporary file" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${actual}" "${fexpected}"
    }

    echo running "${DFS}" --ui "${ui}" --file "${TEST_DATA_DIR}/${input}" cat $extras
    printf "%s" "${expected_text}" > "${fexpected}" || exit 1
    COLUMNS=40
    export COLUMNS
    "${DFS}" --ui "${ui}" --file "${TEST_DATA_DIR}/${input}" "$@" cat $extras > "${actual}"
    rv=$?
    if [ $rv -ne 0 ]
    then
	echo "Exit status $rv for" "${DFS}" --ui "${ui}" --file "${TEST_DATA_DIR}/${input}" "$@" cat
	exit 1
    fi
    if ! diff -u "${fexpected}" "${actual}"
    then
	echo "cat output for file "${input}" with ui ${ui} is incorrect" >&2
	cleanup
	return 1
    else
	cleanup
	return 0
    fi
}

check_cat() {
    check_cat_impl "$@"
    rv=$?
    if [ $rv -eq 0 ]
    then
	result=PASS
    elif [ $rv -gt 0 ]
    then
	result=FAIL
    else
	result=OTHER
    fi
    printf "%20s: %8s ui: %s\n" "$1" "$2" "${result}"
    return $rv
}



check_cat acorn-dfs-ss-80t-manyfiles.ssd acorn "" \
"S0:ABCDEFGHI (30) FM
Drive 0             Option 1 (LOAD)
Dir. :0.$           Lib. :0.$

    EMPTY               S0F01    L
    S0F02    L          S0F03
    S0F04               S0F05    L
    TINY

  %.S0B01             B.S0B01    L
  B.S0B02    L        V.S0B01
" || exit 1

check_cat acorn-dfs-ss-80t-manyfiles.ssd watford "" \
"S0:ABCDEFGHI (30)   Single density
Drive 0             Option 1 (load)
Directory :0.$      Library :0.$
Work file $.

     EMPTY               S0F01    L
     S0F02    L          S0F03
     S0F04               S0F05    L
     TINY

   %.S0B01             B.S0B01    L
   B.S0B02    L        V.S0B01

11 files of 31 on 80 tracks
" || exit 1

check_cat acorn-dfs-sd-80t-empty.ssd acorn "" \
"             (02) FM
Drive 0             Option 2 (RUN)
Dir. :0.$           Lib. :0.$


" || exit 1
# TODO: one of the final newlines above is extraneous.


check_cat opus-ddos-80t-ds-dd-vols-ABCDEFGH.sdd.gz opus "" \
" (01)
 Double density      ABCDEFGH
 Drive 0             Option 0 (off)
 Directory :0.$      Library :0.$


  L.VOL-A
" || exit 1

check_cat opus-ddos-80t-ds-dd-vols-ABCDEFGH.sdd.gz opus 0B \
" (01)
 Double density      ABCDEFGH
 Drive 0B            Option 0 (off)
 Directory :0.$      Library :0.$


  L.VOL-B
" || exit 1
