CXX ?= g++
CPPFLAGS := -Iinclude -Iinclude/memory -Iinclude/reference -Iinclude/tomasulo -Iinclude/common -Iinclude/isa
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

SOURCES := $(shell find src -type f -name '*.cpp' -print)

.PHONY: all clean test test-unit test-data

all: code

code: $(SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $@

test:
	./tests/run_all_tests.sh

test-unit:
	./tests/run_all_tests.sh --unit-only

test-data:
	./tests/run_all_tests.sh --data-only

clean:
	rm -f code
