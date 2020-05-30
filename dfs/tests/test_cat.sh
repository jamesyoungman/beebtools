#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift

# Ensure TMPDIR is set.
: ${TMPDIR:=/tmp}

check_cat() {
    input="$1"
    shift

    dfs() {
	"${DFS}" --file "${TEST_DATA_DIR}/${input}" "$@"
    }

    if ! workdir="$(mktemp --tmpdir=${TMPDIR} -d test_cat_XXXXXX)"
    then
	echo "Unable to create a temporary directory" >&2
	exit 1
    fi

    cleanup() {
	rm -f "${workdir}"/actual.txt "${workdir}/actual0.txt" "${workdir}/actual00.txt"
    }

    (
	cd "${workdir}"
	rv=0
	dfs cat > actual.txt
	dfs cat 0 > actual0.txt
	if diff actual.txt actual0.txt >&2
	then
	    echo "Specifying drive 0 produces the same output, good."
	else
	    echo "FAIL Specifying drive 0 produces different output."
	    rv=1
	fi
	if dfs cat :0ignored > actual00.txt
	then
	    if diff actual0.txt actual00.txt >&2
	    then
		echo "Specifying drive 0 as :0ignored produces the same output, good."
	    else
		echo "FAIL Specifying drive 0 as :0ignored produces different output."
		rv=1
	    fi
	else
	    echo "FAIL specifying drive 0 as :0ignored fails, which it should not." >&2
	    rv=1
	fi

	if dfs cat 1 2>/dev/null
	then
	    echo "FAIL Specifying drive 1 (which should be empty) doesn't fail." >&2
	    rv=1
	fi

	if dfs cat 0 0 2>/dev/null
	then
	    # NOTE: Watford DFS at least allows this.  For example
	    #   *CAT 0 0
	    # produces a catalogue of disc 0, just once.
	    echo "FAIL Specifying more than one drive argument doesn't fail." >&2
	    rv=1
	fi

	for expected
	do
	    if egrep -q -e "${expected}" actual.txt
	    then
		echo "Output matches extended regular expression ${expected}, good"
	    else
		echo "FAIL Output did not match extended regular expression ${expected}" >&2
		rv=1
	    fi
	done
	exit $rv
    )
    rv=$?
    cleanup
    ls -l "${workdir}"
    rmdir "${workdir}"
    ( exit $rv )
}

# NOTHING should not be qualified with $ as that is the current
# directory.  INIT should be shown as locked.
check_cat acorn-dfs-sd-40t.ssd.gz \
	  '[ ]NOTHING' \
	  'INIT +L'

check_cat watford-sd-62-with-62-files.ssd.gz \
	  'Option 0 [(]off[)]' \
	  '^ *62 files of 62$' \
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
