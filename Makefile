CXX ?= g++
CPPFLAGS := -Iinclude -Iinclude/memory -Iinclude/reference -Iinclude/tomasulo -Iinclude/common -Iinclude/isa
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

SOURCES := $(shell find src -type f -name '*.cpp' -print)

.PHONY: all clean

all: code

code: $(SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $@

clean:
	rm -f code
