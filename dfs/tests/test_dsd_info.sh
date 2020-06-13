#! /bin/sh
# Tags: positive

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift


dfs() {
    "${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@"
}

check_info() {
    # The arguments are wildcard - regex pairs.
    # If ${options} is set, that is also included in the command line.
    if ! actual="$(mktemp --tmpdir=${TMPDIR} actual_info_afsp_output_XXXXXX.txt)"
    then
	echo "failed to create temporary file" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${actual}"
    }


    (
	rv=0
	while [ $# -gt 0 ]
	do
	    wildcard="$1"
	    regex="$2"
	    shift 2

	    if ! ${DFS}  \
		 --file "${TEST_DATA_DIR}/dfs-80t-double-sided.dsd" \
		 ${options} info "${wildcard}" > "${actual}"
	    then
		echo "info command failed on ${filename}" >&2
		rv=1
		break
	    fi

	    if egrep -q -e "${regex}" "${actual}"
	    then
		echo ${options} "info ${wildcard} output matches extended regular expression ${regex}, good"
	    else
		echo "FAIL" ${options} "info ${wildcard} output did not match extended regular expression ${regex}." >&2
		if [ -s "${actual}" ]
		then
		    echo "Actual output was:" >&2
		    cat "${actual}" >&2
		else
		    echo "There was no output." >&2
		fi
		rv=1
	    fi
	done
	exit $rv
    )
    rv=$?
    cleanup
    ( exit $rv )
}

options=
check_info ':0.*.*' '[$][.]THISIS0     000800 008023 000029 002' || exit 1
check_info ':2.*.*' '[$][.]THISIS2     000800 008023 000028 002' || exit 1
