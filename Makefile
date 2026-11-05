CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude
LDFLAGS ?= -pthread

SOURCES := $(shell find src -name '*.cpp' -print)
TARGET := build/scapms

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build
