# Contributing to beebtools

Thanks for your help.  Please read on for an explanation of how we can
work effectively together to resolve your problem (or to get your code
change into beebtools).

## Contributing Bug Reports

### Reporting bugs in dfs

First of all, I'm sorry that something didn't work for you.

If the problem is that the dfs program didn't work at all with a disc
image, please include the file in your bug report.  Please also give a
clear description of use of this image file with one of these
emulators:

 - [MAME](https://www.mamedev.org/)
 - [beebjit](https://github.com/scarybeasts/beebjit)
 - [b-em](http://b-em.bbcmicro.com/)

The idea is that, armed only with your description, I can use the disc
image you supply with one of those emulators.

Please ensure that your description is comprehensive.  Please include,
if relevant, the settings of any emulated keyboard DIP switches that
are relevant and which ROMs - including which versions of them - you
are using, and so on.

If the problem is that the dfs program didn't produce the correct
output, but it worked a bit, please include an exact copy of the
output you expected (see "Capturing Output") and explain what's
different.

It is probably helpful to include the output of `dfs` with the
`--verbose` option, which produces a lot more explanation of what is
going on.

### Reporting bugs in bbcbasic_to_text

Please include both a copy of the BBC BASIC file and also the text
output (formatted with LISTO 0, see "Capturing Output") that you
expected, with an explanation of how the actual output differs.

### Capturing Output

You can generate an exact copy of the output you expected by combining
the "printer output capture" feature of an emulator and the use of
"VDU 2" to send LIST, *CAT, *INFO etc. output to the printer.

## Contributing Code

Code contributions are very welcome indeed.  Please make sure that all
the test pass on your new code.  If you are adding code please ensure
that your new code has tests.

The code is licensed under the Apache 2.0.  Submissions of code must
also be under this license (see COPYING, section 5).
