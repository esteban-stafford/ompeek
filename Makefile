# Compiler and flags
CC      := clang
CFLAGS  := -Wall -O2 -fopenmp
LDFLAGS := -lm

# Source and binary lists
SRCS    := $(filter-out ompt_tool_event.c, $(wildcard *.c))
BINS    := $(SRCS:.c=) libompt_tool_event.so

# Default target: build all binaries
all: $(BINS)

burst_viewer_embed.cpp: burst_viewer_embed.html embed_viewer
	perl embed_viewer $< $@

libompt_tool_event.so: ompt_tool_event.cpp burst_viewer_embed.cpp
	clang++ -fPIC -shared -o libompt_tool_event.so $<

# Rule to compile each .c into a binary
%: %.c burst.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Clean up
clean:
	rm -f $(BINS)
