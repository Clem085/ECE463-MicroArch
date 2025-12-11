CXX := g++
CXXFLAGS := -O3 -Wall -Wextra -pedantic -std=c++17

SOURCES := sim.cc
OBJECTS := $(SOURCES:.cc=.o)

.PHONY: all clean

all: sim
	@echo "Build complete."

sim: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)

%.o: %.cc sim.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) sim
