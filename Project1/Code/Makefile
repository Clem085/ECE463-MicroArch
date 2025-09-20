# Root-style Makefile (works like Gradescope)
# Builds ./sim from all .cc files in the current directory.

CXX       := g++
CXXFLAGS  := -std=c++17 -O3 -Wall -Wextra
LDFLAGS   :=
LDLIBS    :=

SOURCES   := $(wildcard *.cc)
OBJECTS   := $(SOURCES:.cc=.o)
TARGET    := sim

# traces we’ll “stage” to the current directory for Gradescope-like runs
TRACES_SRC := traces
TRACE_FILES := gcc_trace.txt go_trace.txt perl_trace.txt compress_trace.txt vortex_trace.txt

.PHONY: all clean stage run val1 val2 val3 val4 allvals

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) my_val*.txt

# --- Make local behave like Gradescope ---
# Gradescope places trace files at the submission root.
# 'stage' copies them from ./traces/ into the current directory.
stage:
	@for f in $(TRACE_FILES); do \
		if [ -f "$(TRACES_SRC)/$$f" ]; then cp -f "$(TRACES_SRC)/$$f" .; fi; \
	done

# Convenience targets; depend on sim and stage so they run like the grader
run: stage $(TARGET)
	./$(TARGET) 32 8192 4 0 0 0 0 gcc_trace.txt

val1: stage $(TARGET)
	./$(TARGET) 16 1024 1 0 0 0 0 gcc_trace.txt > my_val1.txt
	diff -iw my_val1.txt val-proj1/val1.16_1024_1_0_0_0_0_gcc.txt

val2: stage $(TARGET)
	./$(TARGET) 32 1024 2 0 0 0 0 gcc_trace.txt > my_val2.txt
	diff -iw my_val2.txt val-proj1/val2.32_1024_2_0_0_0_0_gcc.txt

val3: stage $(TARGET)
	./$(TARGET) 16 1024 1 8192 4 0 0 gcc_trace.txt > my_val3.txt
	diff -iw my_val3.txt val-proj1/val3.16_1024_1_8192_4_0_0_gcc.txt

val4: stage $(TARGET)
	./$(TARGET) 32 1024 2 6144 3 0 0 gcc_trace.txt > my_val4.txt
	diff -iw my_val4.txt val-proj1/val4.32_1024_2_6144_3_0_0_gcc.txt

allvals: val1 val2 val3 val4
