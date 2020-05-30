#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

check_dump() {
    input="$1"
    shift
    # The rest of the arguments are filename - regex pairs.

    if ! actual="$(mktemp --tmpdir=${TMPDIR} actual_dump_output_XXXXXX.txt)"
    then
	echo "failed to create temporary file" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${input}" "${actual}"
    }

    (
	rv=0
	while [ $# -gt 0 ]
	do
	    filename="$1"
	    regex="$2"
	    shift 2
	    if ! ${DFS}  --file "${TEST_DATA_DIR}/${input}" dump "${filename}" > "${actual}"
	    then
		echo "dump command failed on ${filename}" >&2
		rv=1
		break
	    fi
	    if egrep -q -e "${regex}" "${actual}"
	    then
		echo "dump ${filename} output matches extended regular expression ${regex}, good"
	    else
		echo "FAIL dump ${filename} output did not match extended regular expression ${regex}:" >&2
		cat "${actual}" >&2
		rv=1
	    fi
	done
	exit $rv
    )
    rv=$?
    cleanup
    ( exit $rv )
}

# The space in "FIFTY FOUR" should be rendered as a space, not a dot.
# After the end of the file the hex digits column should contain **
# to denote that the data is absent.  Also check that the line
# break occurs at the correct point (i.e. that counting from 0, byte
# 8 of the file is the first thing on the secondf line).
check_dump watford-sd-62-with-62-files.ssd.gz \
	   'FILE54' 'FIFTY' \
	   'FILE54' 'FIFTY FO' \
	   'FILE54' '^000008 55 52 0D [*][*]'
