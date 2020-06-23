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

check_cat() {
    input="$1"
    ui="$2"
    expected_text="$3"
    shift 3

    dfs() {
	COLUMNS=40 "${DFS}" --ui "${ui}" --file "${TEST_DATA_DIR}/${input}" "$@"
    }

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

    printf "%s" "${expected_text}" > "${fexpected}" || exit 1
    if ! dfs cat > "${actual}"
    then
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

check_cat \
    acorn-dfs-ss-80t-manyfiles.ssd \
    acorn \
"S0:ABCDEFGHI (26) FM
Drive 0             Option 0 (off)
Dir. :0.$           Lib. :0.$

    S0F01    L          S0F02    L
    S0F03               S0F04
    S0F05    L

  %.S0B01             B.S0B01    L
  B.S0B02    L        V.S0B01
"

check_cat \
    acorn-dfs-ss-80t-manyfiles.ssd \
    watford \
"S0:ABCDEFGHI (26)   Single density
Drive 0             Option 0 (off)
Directory :0.$      Library :0.$
Work file $.

     S0F01    L          S0F02    L
     S0F03               S0F04
     S0F05    L

   %.S0B01             B.S0B01    L
   B.S0B02    L        V.S0B01

09 files of 31 on 80 tracks
"
