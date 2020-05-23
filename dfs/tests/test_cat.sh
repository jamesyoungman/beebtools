#! /bin/sh

# Args:
# ${DFS}" "${TEST_DATA_DIR}"
DFS="$1"
shift
TEST_DATA_DIR="$1"
shift


check_cat() {
    input="$1"
    shift

    cleanup() {
	rm -f "${input}" expected.txt actual.txt
    }

    dfs() {
	"${DFS}" --file "${input}" "$@"
    }

    gunzip < "${TEST_DATA_DIR}/${input}.gz" > "${input}" || exit 1

    (
	rv=0
	dfs cat > actual.txt
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
    ( exit $rv )
}

# NOTHING should not be qualified with $ as that is the current
# directory.  INIT should be shown as locked.
check_cat acorn-dfs-sd-40t.ssd \
	  '[ ]NOTHING' \
	  'INIT +L'

check_cat watford-sd-62-with-62-files.ssd \
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
