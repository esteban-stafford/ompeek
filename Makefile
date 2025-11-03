# Compiler and flags
CC      := clang
CFLAGS  := -Wall -O2 -fopenmp
LDFLAGS := -lm

# Source and binary lists
SRCS    := $(filter-out ompt_tool_event.c, $(wildcard *.c))
BINS    := $(SRCS:.c=) libompt_tool_event.so

# Default target: build all binaries
all: $(BINS)

libompt_tool_event.so: ompt_tool_event.cpp
	clang++ -fPIC -shared -o libompt_tool_event.so $<

# Rule to compile each .c into a binary
%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Clean up
clean:
	rm -f $(BINS)
