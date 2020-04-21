RM = rm -f
CXXFLAGS = -ggdb -Wall -Wextra -std=c++17
CPPFLAGS_TESTS = -DVERBOSE_FOR_TESTS=1
TESTS = test_afsp test_stringutil
TARGETS = dfs $(TESTS)
HEADERS = dfscontext.h afsp.h fsp.h stringutil.h regularexpression.h dfsimage.h

COMMON_SOURCES = afsp.cc fsp.cc stringutil.cc dfsimage.cc
DFS_SOURCES = dfs.cc $(COMMON_SOURCES)

default: $(TARGETS)

everything-compiles: $(TARGETS)

clean:
	$(RM) $(TARGETS)

dfs: $(DFS_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(DFS_SOURCES)

test_afsp: test_afsp.cc $(COMMON_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS_TESTS) -o $@ test_afsp.cc afsp.cc fsp.cc stringutil.cc

test_stringutil: test_stringutil.cc $(COMMON_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS_TESTS) -o $@ test_stringutil.cc stringutil.cc

check: everything-compiles $(TESTS)
	for testprogram in $(TESTS); do echo running $${testprogram}... && ./$${testprogram} || exit 1; done
