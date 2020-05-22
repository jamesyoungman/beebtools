#! /bin/sh
cmd="$@"
if ! "$@"; then
    # As desired.
    exit 0
fi
echo "Expected $@ to exit unsussessfully, but it succeeded instead." >&2
exit 1
