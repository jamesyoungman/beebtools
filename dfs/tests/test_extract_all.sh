#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

if ! extract_dfs=$(mktemp -d tmp_dfs.XXXXXX)
then
    echo "Unable to create a temporary directory" >&2
    exit 1
fi
if ! extract_zip=$(mktemp -d tmp_zip.XXXXXX)
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

    gunzip < "${TEST_DATA_DIR}/${input}.gz" > "${input}" || exit 1

    (
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
	echo after extracting the zip file:
	ls "${extract_zip}"

	# Delete the .inf files to make it more convenient to compare
	# results using diff -r.
	if ! rm "${extract_zip}"/*.inf "${extract_dfs}"/*.inf
	then
	    echo "FAILED to delete inf files" >&2
	    exit 1
	fi
	echo after deleting the inf files:
	ls "${extract_zip}"

	if ! diff -r "${extract_dfs}" "${extract_zip}" >&2
	then
		    echo "FAILED: ectracted files differ" >&2
		    exit 1
	fi
    )
    rv=$?
    cleanup
    ( exit $rv )
}

check_extract watford-sd-62-with-62-files.ssd  watford-sd-62-with-62-files.zip
