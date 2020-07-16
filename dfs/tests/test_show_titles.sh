DFS="$1"
#
#   Copyright 2020 James Youngman
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
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
