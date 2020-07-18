# Building beebtools

The software can be built (and tested) with cmake, like this:

```sh
mkdir _build
cd _build
cmake .. && make && ctest
```

## Build Options

The following options exist:

### COVERAGE

Generate code coverage statistics.  Turn this on with `cmake
-DCOVERAGE=ON`.  For instructions on how to use this option, see below.

### IWYU

Run the include-what-you-use static analyzer when building the code.
Turn this on with `cmake -DIWYU=ON`.

NOTE: the iwyu checks are not all correct, so this is not recommended
as a default.

### PDF

Generate PDF documentation.   Turn this on with `cmake -DPDF=ON`.

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
