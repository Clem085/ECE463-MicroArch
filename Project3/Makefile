CXX := g++
CXXFLAGS := -O3 -std=c++17 -Wall -Wextra

SRC := sim.cc
OBJ := $(SRC:.cc=.o)

all: sim

sim: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cc sim.h
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJ) sim

.PHONY: all clean
