# ////// MODIFIED MAKE FILE ////// #
# Top-level Makefile (~/MicroArch/Project1/Code)

CXX ?= g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra

SRC_DIR = read-trace/cpp_files
SRCS    = $(wildcard $(SRC_DIR)/*.cc)
OBJS    = $(SRCS:.cc=.o)
BIN     = sim

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BIN)



.PHONY: run val1 val2 val3 val4 # val5 val6 val7 val8 allvals

run:
	./sim 32 8192 4 0 0 0 0 traces/example_trace.txt

val1:
	./sim 16 1024 1 0 0 0 0 traces/gcc_trace.txt > my_val1.txt
	diff -iw my_val1.txt val-proj1/val1.16_1024_1_0_0_0_0_gcc.txt

val2:
	./sim 32 1024 2 0 0 0 0 traces/gcc_trace.txt > my_val2.txt
	diff -iw my_val2.txt val-proj1/val2.32_1024_2_0_0_0_0_gcc.txt

val3:
	./sim 16 1024 1 8192 4 0 0 traces/gcc_trace.txt > my_val3.txt
	diff -iw my_val3.txt val-proj1/val3.16_1024_1_8192_4_0_0_gcc.txt

val4:
	./sim 32 1024 2 6144 3 0 0 traces/gcc_trace.txt > my_val4.txt
	diff -iw my_val4.txt val-proj1/val4.32_1024_2_6144_3_0_0_gcc.txt

# val5:
# 	./sim 16 1024 1 0 0 1 4 traces/gcc_trace.txt > my_val5.txt
# 	diff -iw my_val5.txt val-proj1/val5.16_1024_1_0_0_1_4_gcc.txt

# val6:
# 	./sim 32 1024 2 0 0 3 1 traces/gcc_trace.txt > my_val6.txt
# 	diff -iw my_val6.txt val-proj1/val6.32_1024_2_0_0_3_1_gcc.txt

# val7:
# 	./sim 16 1024 1 8192 4 3 4 traces/gcc_trace.txt > my_val7.txt
# 	diff -iw my_val7.txt val-proj1/val7.16_1024_1_8192_4_3_4_gcc.txt

# val8:
# 	./sim 32 1024 2 12288 6 7 6 traces/gcc_trace.txt > my_val8.txt
# 	diff -iw my_val8.txt val-proj1/val8.32_1024_2_12288_6_7_6_gcc.txt

allvals: val1 val2 val3 val4 # val5 val6 val7 val8
# ////// MODIFIED MAKE FILE ////// #



# ////// ORIG MAKE FILE ////// #
# CC = g++
# OPT = -O3
# #OPT = -g
# WARN = -Wall
# # You can select a C++ standard using the STD define below.  To do so, uncomment (remove leading #) and adjust the standard as needed.
# #STD = -std=c++11
# CFLAGS = $(OPT) $(WARN) $(STD) $(INC) $(LIB)

# # List all your .cc/.cpp files here (source files, excluding header files)
# SIM_SRC = sim.cc

# # List corresponding compiled object files here (.o files)
# SIM_OBJ = sim.o
 
# #################################

# # default rule

# all: sim
# 	@echo "my work is done here..."


# # rule for making sim

# sim: $(SIM_OBJ)
# 	$(CC) -o sim $(CFLAGS) $(SIM_OBJ) -lm
# 	@echo "-----------DONE WITH sim-----------"


# # generic rule for converting any .cc file to any .o file
 
# .cc.o:
# 	$(CC) $(CFLAGS) -c $*.cc

# # generic rule for converting any .cpp file to any .o file

# .cpp.o:
# 	$(CC) $(CFLAGS) -c $*.cpp


# # type "make clean" to remove all .o files plus the sim binary

# clean:
# 	rm -f *.o sim


# # type "make clobber" to remove all .o files (leaves sim binary)

# clobber:
# 	rm -f *.o
# ////// ORIG MAKE FILE ////// #