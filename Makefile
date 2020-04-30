RM = rm -f
CXXFLAGS = -ggdb -Wall -Wextra -std=c++17
CFLAGS = -ggdb -Wall -Wextra

DFS_TESTS = dfs/test_afsp dfs/test_stringutil
DFS_TARGETS = dfs/dfs dfs/splitimage $(DFS_TESTS)
DFS_HEADERS = dfs/dfscontext.h dfs/afsp.h dfs/fsp.h dfs/stringutil.h dfs/regularexpression.h dfs/dfsimage.h dfs/storage.h dfs/dfstypes.h dfs/commands.h
DFS_COMMON_SOURCES = dfs/afsp.cc dfs/fsp.cc dfs/stringutil.cc dfs/dfsimage.cc dfs/storage.cc
DFS_SOURCES = dfs/dfs.cc dfs/cmd_cat.cc dfs/cmd_type.cc dfs/cmd_list.cc  dfs/commands.cc $(DFS_COMMON_SOURCES)

BASIC_TESTS = basic/tokens_test 
BASIC_TARGETS = basic/bbcbasic_to_text  

default: $(DFS_TARGETS) $(BASIC_TARGETS)

everything-compiles: $(DFS_TARGETS)

clean: dfs-clean basic-clean

check: dfs-check basic-check

dfs-clean:
	$(RM) $(DFS_TARGETS)

dfs/dfs: $(DFS_SOURCES) $(DFS_HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(DFS_SOURCES)

dfs/splitimage: dfs/splitimage.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

dfs/test_afsp: dfs/test_afsp.cc $(DFS_COMMON_SOURCES) $(DFS_HEADERS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS_TESTS) -o $@ dfs/test_afsp.cc dfs/afsp.cc dfs/fsp.cc dfs/stringutil.cc

dfs/test_stringutil: dfs/test_stringutil.cc $(DFS_COMMON_SOURCES) $(DFS_HEADERS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS_TESTS) -o $@ dfs/test_stringutil.cc dfs/stringutil.cc

dfs-check: $(DFS_TESTS)
	for testprogram in $(DFS_TESTS); do echo running $${testprogram}... && ( cd dfs && ../$${testprogram} ) || exit 1; done



basic-clean:
	$(RM) $(BASIC_TARGETS) basic/bbcbasic_to_text.o basic/tokens.o basic/tokens_test.o

basic-check: basic-check_tokens basic-check_golden

basic/tokens.o: basic/tokens.c basic/tokens.h
basic/tokens_test.o: basic/tokens_test.c basic/tokens.h
basic/tokens_test: basic/tokens_test.o basic/tokens.o


basic-check_tokens: basic/tokens_test
	basic/tokens_test

basic-check_golden: basic/bbcbasic_to_text basic/run_golden_tests.sh
	( cd basic && sh run_golden_tests.sh ./bbcbasic_to_text testdata )

basic/bbcbasic_to_text: basic/bbcbasic_to_text.o basic/tokens.o
	$(CC) $(CFLAGS) -o $@ basic/bbcbasic_to_text.o basic/tokens.o
