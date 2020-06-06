#! /bin/sh
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
    if "${DFS}" help "${nope}"
    then
	echo "Failed to diagnose problem with help ${nope}" >&2
	rv=1
    fi

    exit $rv
)
rv=$?
rm -f "${t}"
( exit $rv )
