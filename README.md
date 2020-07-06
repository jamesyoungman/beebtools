# beebtools README

## What Is This?

A set of tools for working with discs and files
from computers in the BBC Microcomputer family.

It includes

1.  dfs: a program for extracting data from Acorn DFS format floppy discs (or more typically, floppy disc images)
2.  bbcbasic_to_text: a program which converts a saved BBC basic program to text

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
