# Compiler and flags
CC      := clang
CFLAGS  := -Wall -O2 -fopenmp
LDFLAGS := -lm

# Source and binary lists
SRCS    := $(filter-out ompeek.c, $(wildcard *.c))
BINS    := $(SRCS:.c=) libompeek.so

# Default target: build all binaries
all: $(BINS)

burst_viewer_embed.cpp: burst_viewer_embed.html embed_viewer
	perl embed_viewer $< $@

libompeek.so: ompeek.cpp burst_viewer_embed.cpp
	clang++ -fPIC -shared -o libompeek.so $<

# Rule to compile each .c into a binary
%: %.c burst.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Clean up
clean:
	rm -f $(BINS)
