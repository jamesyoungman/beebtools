#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

if ! extract_dfs=$(mktemp --tmpdir="${TMPDIR}" -d 'tmp_dfs.XXXXXX' )
then
    echo "Unable to create a temporary directory" >&2
    exit 1
fi
if ! extract_zip=$(mktemp  --tmpdir="${TMPDIR}" -d 'tmp_zip.XXXXXX' )
then
    echo "Unable to create a temporary directory" >&2
    exit 1
fi

check_extract() {
    input="$1"
    shift
    archive="$1"
    shift

    cleanup() {
	rm -f "${input}" "${extract_dfs}"/* "${extract_zip}"/*
    }

    dfs() {
	"${DFS}" --file "${input}" "$@"
    }

    (
	cleanup
	gunzip < "${TEST_DATA_DIR}/${input}.gz" > "${input}" || exit 1

	if ! dfs extract-files "${extract_dfs}"
	then
	    echo "FAILED: extract-files returned nonzero" >&2
	    exit 1
	fi
	zipfile="${TEST_DATA_DIR}/${archive}"
	if ! ( cd "${extract_zip}" && unzip "${zipfile}" )
	then
	    echo "FAILED: could not extract ${zipfile}" >&2
	    exit 1
	fi

	# Spot-check an inf-file.
	rx='^[$][.]FILE48 +FFFFFF +FFFFFF +0+C +CRC=[0-9A-F]+'
	if egrep "${rx}"  < "${extract_dfs}"/FILE48.inf
	then
	    echo "${extract_dfs}/FILE48.inf matches ${rx}, good." >&2
	else
	    echo "FAIL: Expected ${extract_dfs}/FILE48.inf to match ${rx} dut it doesn't." >&2
	    cat ${extract_dfs}/FILE48.inf >&2; echo
	    exit 1
	fi
	len="$(wc -c < ${extract_zip}/FILE48.inf)"
	if [ "${len}" -ne 39 ]
	then
	    echo "FAIL: Expected ${extract_zip}/FILE48.inf to be 39 bytes long, it's actually $len" >&2
	    exit 1
	fi

	# Delete the .inf files to make it more convenient to compare
	# results using diff -r.
	if ! rm "${extract_zip}"/*.inf "${extract_dfs}"/*.inf
	then
	    echo "FAILED to delete inf files" >&2
	    exit 1
	fi

	if ! diff -r "${extract_dfs}" "${extract_zip}" >&2
	then
		    echo "FAILED: extracted DFS files differ" >&2
		    exit 1
	fi


	# Perform the extration operations again, this time deleting
	# all the files other than the .inf files, so that we can
	# compare just those.
	cleanup
	gunzip < "${TEST_DATA_DIR}/${input}.gz" > "${input}" || exit 1

	if ! ( cd "${extract_zip}" && unzip "${zipfile}" )
	then
	    echo "FAILED: could not extract ${zipfile}" >&2
	    exit 1
	fi
	if ! dfs extract-files "${extract_dfs}"
	then
	    echo "FAILED: extract-files returned nonzero" >&2
	    exit 1
	fi

	echo "Deleting non-inf files in ${extract_dfs}"
	rm -f "${extract_dfs}"/F*0
	rm -f "${extract_dfs}"/F*1
	rm -f "${extract_dfs}"/F*2
	rm -f "${extract_dfs}"/F*3
	rm -f "${extract_dfs}"/F*4
	rm -f "${extract_dfs}"/F*5
	rm -f "${extract_dfs}"/F*6
	rm -f "${extract_dfs}"/F*7
	rm -f "${extract_dfs}"/F*8
	rm -f "${extract_dfs}"/F*9

	echo "Deleting non-inf files in ${extract_zip}"
	rm -f "${extract_zip}"/F*0
	rm -f "${extract_zip}"/F*1
	rm -f "${extract_zip}"/F*2
	rm -f "${extract_zip}"/F*3
	rm -f "${extract_zip}"/F*4
	rm -f "${extract_zip}"/F*5
	rm -f "${extract_zip}"/F*6
	rm -f "${extract_zip}"/F*7
	rm -f "${extract_zip}"/F*8
	rm -f "${extract_zip}"/F*9

	if ! diff -r "${extract_dfs}" "${extract_zip}" >&2
	then
		    echo "FAILED: extracted .inf files differ" >&2
		    exit 1
	fi


    )
    rv=$?
    cleanup
    ( exit $rv )
}

check_extract watford-sd-62-with-62-files.ssd  watford-sd-62-with-62-files.zip
