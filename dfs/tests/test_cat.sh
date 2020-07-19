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

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:?}


check_cat() {
    input="$1"
    shift

    dfs() {
	COLUMNS=40 "${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@"
    }

    if ! workdir="$(mktemp --tmpdir=${TMPDIR:?} -d test_cat_XXXXXX)"
    then
	echo "Unable to create a temporary directory" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${workdir}"/actual.txt "${workdir}/actual0.txt" "${workdir}/actual00.txt"
    }

    (
	set -x
	cd "${workdir}"
	rv=0
	dfs cat > actual.txt
	dfs cat 0 > actual0.txt
	if diff actual.txt actual0.txt >&2
	then
	    echo "Specifying drive 0 produces the same output, good."
	else
	    echo "FAIL Specifying drive 0 produces different output." >&2
	    exit 1
	fi

	# "dfs cat :0ignored" was previously ignored but this is
	# incompatible with the need to be able to parse Opus DDOS
	# volume letters (i.e. 'cat 0B' needs to be possible in the
	# future).

	if ! fails dfs cat 1 2>/dev/null
	then
	    echo "FAIL Specifying drive 1 (which should be empty) doesn't fail." >&2
	    exit 1
	fi

	if ! fails dfs cat 0 0 2>/dev/null
	then
	    # NOTE: Watford DFS at least allows this.  For example
	    #   *CAT 0 0
	    # produces a catalogue of disc 0, just once.
	    echo "FAIL Specifying more than one drive argument doesn't fail." >&2
	    exit 1
	fi

	for expected
	do
	    if egrep -q -e "${expected}" actual.txt
	    then
		echo "Output matches extended regular expression ${expected}, good"
	    else
		echo "FAIL Output did not match extended regular expression ${expected}" >&2
		echo "Actual output was:"; cat actual.txt
		exit 1
	    fi
	done
	exit 0
    )
    rv=$?
    echo "test_cat: check_cat subshell exited with status $rv" >&2
    cleanup
    rmdir "${workdir}"
    ( exit $rv )
}

# NOTHING should not be qualified with $ as that is the current
# directory.  INIT should be shown as locked.
check_cat acorn-dfs-sd-40t.ssd.gz \
	  '[ ]NOTHING' \
	  'INIT +L'

# TODO: ensure geometry guessing works everywhere
# and make the "on 80 tracks" part of the regex
# below mandatory.
check_cat watford-sd-62-with-62-files.ssd.gz \
	  'Option 0 [(]off[)]' \
	  '^ *62 files of 62( on 80 tracks)?$' \
	  "^ +FILE01 +FILE02 *$" \
	  "^ +FILE03 +FILE04 *$" \
	  "^ +FILE05 +FILE06 *$" \
	  "^ +FILE07 +FILE08 *$" \
	  "^ +FILE09 +FILE10 *$" \
	  "^ +FILE11 +FILE12 *$" \
	  "^ +FILE13 +FILE14 *$" \
	  "^ +FILE15 +FILE16 *$" \
	  "^ +FILE17 +FILE18 *$" \
	  "^ +FILE19 +FILE20 *$" \
	  "^ +FILE21 +FILE22 *$" \
	  "^ +FILE23 +FILE24 *$" \
	  "^ +FILE25 +FILE26 *$" \
	  "^ +FILE27 +FILE28 *$" \
	  "^ +FILE29 +FILE30 *$" \
	  "^ +FILE31 +FILE32 *$" \
	  "^ +FILE33 +FILE34 *$" \
	  "^ +FILE35 +FILE36 *$" \
	  "^ +FILE37 +FILE38 *$" \
	  "^ +FILE39 +FILE40 *$" \
	  "^ +FILE41 +FILE42 *$" \
	  "^ +FILE43 +FILE44 *$" \
	  "^ +FILE45 +FILE46 *$" \
	  "^ +FILE47 +FILE48 *$" \
	  "^ +FILE49 +FILE50 *$" \
	  "^ +FILE51 +FILE52 *$" \
	  "^ +FILE53 +FILE54 *$" \
	  "^ +FILE55 +FILE56 *$" \
	  "^ +FILE57 +FILE58 *$" \
	  "^ +FILE59 +FILE60 *$" \
	  "^ +FILE61 +FILE62 *$"
