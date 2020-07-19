#! /bin/sh
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
# Tags: positive

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:?}

check_info() {
    # The arguments are wildcard - regex pairs.
    # If ${options} is set, that is also included in the command line.
    if ! actual="$(mktemp --tmpdir=${TMPDIR:?} actual_info_afsp_output_XXXXXX.txt)"
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
		 --file "${TEST_DATA_DIR}/acorn-dfs-sd-40t.ssd.gz" \
		 --file "${TEST_DATA_DIR}/acorn-dfs-ss-80t-textfiles.ssd.gz" \
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

options="--drive 0"
check_info ':0.#.*' 'B.NOTHING     FF1900 FF8023 000064 003'
options="--drive 1"
check_info ':0.#.*' 'B.NOTHING     FF1900 FF8023 000064 003'
