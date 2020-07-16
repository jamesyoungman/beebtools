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
# Tags: positive negative

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

if ! t="$(mktemp --tmpdir=${TMPDIR})"; then
    echo 'Failed to make temporary file' >&2
    exit 1
fi
(
    rv=0
    commands="cat dump extract-files extract-unused free help info list sector-map show-titles space type"

    check() {
	for c in $commands
	do
	    pat="^ *${c} *:"
	    if egrep -q "${pat}" < "${t}"
	    then
		echo "help output includes command ${c}, good"
	    else
		echo "help output does not include command ${c} (that is, does not match ${pat}):" >&2
		cat "${t}" >&2
		rv=1
	    fi
	done
    }

    "${DFS}" help >| "${t}"
    check
    "${DFS}" --help >| "${t}"
    check
    "${DFS}" help $commands >| "${t}"
    check


    cmd_re='^ *[a-z][-_a-z]* *:'
    cmd_count=$(${DFS} help | grep -v 'usage:' | egrep -c -e "${cmd_re}")
    set -- $commands
    if [ $cmd_count -ne $# ]
    then
	echo "Expected there to be $# commands, but found ${cmd_count}:" >&2
	${DFS} help | grep -v 'usage:' | egrep -e "${cmd_re}" | nl -ba
	rv=1
    fi

    # Verify that we diagnose a problem for a nonexistent command.
    nope="this-command-does-not-exist"
    if ! fails "${DFS}" help "${nope}"
    then
	echo "Failed to diagnose problem with help ${nope}" >&2
	rv=1
    fi

    exit $rv
)
rv=$?
rm -f "${t}"
( exit $rv )
