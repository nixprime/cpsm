CCFLAGS := -std=c++11 -Wall -pthread

.PHONY: alltests
alltests: test/matcher_test

test/matcher_test: src/matcher_test.cc src/matcher.cc src/path_util.cc src/str_util.cc
	mkdir -p test
	$(CXX) -o $@ $(CCFLAGS) $^
