DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}


check_show_titles() {
    image="$1"
    spec="$2"
    expected="$3"
    shift 3

    dfs() {
	"${DFS}" --file "${TEST_DATA_DIR}/${image}" "$@"
    }

    if ! actual="$(mktemp --tmpdir=${TMPDIR} test_show_titles_actual_XXXXXX)"
    then
	echo "Unable to create a temporary file to store actual output" >&2
	exit 1
    fi
    if ! te="$(mktemp --tmpdir=${TMPDIR} test_show_titles_expected_XXXXXX)"
    then
	echo "Unable to create a temporary file to store expected output" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${actual}" "${te}"
    }

    (
	rv=0
	if dfs show-titles "$spec" >"${actual}"
	then
	    if [ "$( cat ${actual} )" != "${expected}" ]
	    then
		(
		printf '%s' "${expected}"  > "${te}"
		if ! diff -u "${te}" "${actual}"
		then
		    exec >&2
		    echo "show-titles --file ${image} ${spec} gave incorrect result."
		    echo 'expected:'
		    printf '%s' "${expected}"
		    echo 'got:'
		    cat  "${actual}"
		    exit 1
		fi
		) || rv=1
	    fi
	else
	    echo "show-titles --file ${image} ${spec} failed." >&2
	    exit 1
	fi
	cleanup
	exit $rv
    )
}


check_show_titles acorn-dfs-ss-80t-manyfiles.ssd 0 "0: S0:ABCDEFGHI" || exit 1

check_show_titles opus-ddos.sdd.gz 0 \
"0A: OPUS80DD
0B: OPUS-VOLB
0C: OPUS-VOLC
0D: OPUS-VOLD
0E: OPUS-VOLE
" || exit 1
