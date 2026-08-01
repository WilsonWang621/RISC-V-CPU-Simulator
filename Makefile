CXX ?= g++
CPPFLAGS := -Iinclude -Iinclude/memory -Iinclude/reference
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

SOURCES := \
	src/main.cpp \
	src/isa/decoder.cpp \
	src/memory/image.cpp \
	src/reference/interpreter.cpp

.PHONY: all clean

all: code

code: $(SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $@

clean:
	rm -f code
