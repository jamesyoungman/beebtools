# beebtools README

## What Is This?

A set of tools for working with discs and files
from computers in the BBC Microcomputer family.

It includes

1.  dfs: a program for extracting data from Acorn DFS format floppy disc images
2.  bbcbasic_to_text: a program which converts a saved BBC basic program to text

## I've seen similar things before, what's interesting about this one?

The BBC BASIC de-tokeniser `bbcbasic_to_text` is quite comprehensive:

1. It covers less well-known implementations (Mac Classic, PDP-11) as
   well as the traditional (BBC Micro, Archimedes, Z80) and modern
   (Windows, SDL) implementatons.
2. It handles a number of situations better than some of the
   alternative programs:
   1. Line numbers
   2. Lines lacking line numbers
   3. It incorporates (and documents) some obscure issues not
      documented elsewhere

The `dfs` program deals well with a number of less-common cases:

1. Double-density disks
1. Support for Opus DDOS and Watford DFS discs as well as, of course,
   Acorn DFS floppies.
1. As well as SSD, SDD, DSD and DDD image files, MMB files and HFE
   versions 1 and 3 are suported.
1. All supported image file formats are supported compressed (with
   gzip).
1. Generally, file system type and disc geometry guessing are quite
   thorough.  For example, the code can successfully identify an Opus
   DDOS 35-track double-density disc image.
1. The code explains (optionally, via --verbose) why it does or does
   not believe a particular image file contains any of the supported
   file systems.

Manual pages and regression tests are included.


## How Do I Build It?

The software can be built (and tested) with cmake, like this:

```sh
mkdir _build
cd _build
cmake .. && make && ctest
```

## Build Options

The following options exist:

### PDF

Generate PDF documentation.   Turn this on with `cmake -DPDF=ON`.

### COVERAGE

Generate code coverage statistics.  Turn this on with `cmake
-DCOVERAGE=ON`.  For instructions on how to use this option, see below.

## Code Coverage

To measure code coverage you will need to download
[CodeCoverage.cmake](https://github.com/bilke/cmake-modules/blob/master/CodeCoverage.cmake)
into a directory that you put in CMAKE_MODULE_PATH.   Here is a worked example:

```sh
mkdir -p /tmp/coverage &&
wget -O /tmp/coverage/CodeCoverage.cmake \
  'https://github.com/bilke/cmake-modules/raw/master/CodeCoverage.cmake' &&
( mkdir -p _build && cd _build &&
  cmake -DCMAKE_MODULE_PATH=/tmp/coverage/ -DCOVERAGE=ON -DCMAKE_BUILD_TYPE=Debug .. &&
  make && make coverage
)
```

The coverage report ends up in `_build/coverage/index.html`.
