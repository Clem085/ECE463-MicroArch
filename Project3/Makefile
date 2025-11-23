CXX := g++
CXXFLAGS := -O3 -std=c++17 -Wall -Wextra

SRC := sim.cc
OBJ := $(SRC:.cc=.o)
SIM_PROC_SRC := project3_read_trace/cpp_files/sim_proc.cc
SIM_PROC_OBJ := sim_proc.o

all: sim

sim: $(OBJ) $(SIM_PROC_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cc sim.h
	$(CXX) $(CXXFLAGS) -c $<

$(SIM_PROC_OBJ): $(SIM_PROC_SRC) project3_read_trace/cpp_files/sim_proc.h
	$(CXX) $(CXXFLAGS) -Dmain=sim_proc_stub -c $(SIM_PROC_SRC) -o $@

clean:
	rm -f $(OBJ) $(SIM_PROC_OBJ) sim

.PHONY: all clean
