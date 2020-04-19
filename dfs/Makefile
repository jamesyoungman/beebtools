RM = rm -f
CXXFLAGS = -ggdb -Wall -Wextra -std=c++17
CPPFLAGS_TESTS = -DVERBOSE_FOR_TESTS=1
TESTS = test_afsp
TARGETS = dfs $(TESTS)
HEADERS = dfscontext.h afsp.h

default: $(TARGETS)

clean:
	$(RM) $(TARGETS)

dfs: dfs.cc $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ dfs.cc

test_afsp: test_afsp.cc afsp.cc $(HEADERS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS_TESTS) -o $@ test_afsp.cc afsp.cc

check: $(TESTS)
	for testprogram in $(TESTS); do echo running $${testprogram}... && ./$${testprogram} || exit 1; done
