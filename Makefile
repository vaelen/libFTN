# libFTN Makefile
# Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>

CC = cc
CFLAGS = -Wall -Wextra -pedantic -std=c89 -O2
INCLUDES = -Iinclude
LIBDIR = lib
BINDIR = bin
OBJDIR = obj
SRCDIR = src
TESTDIR = tests

# Library name
LIBRARY = $(LIBDIR)/libftn.a

# Source files
SOURCES = $(SRCDIR)/ftn.c $(SRCDIR)/crc.c $(SRCDIR)/nodelist.c $(SRCDIR)/search.c $(SRCDIR)/compat.c
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Test programs
TEST_SOURCES = $(TESTDIR)/test_nodelist.c $(TESTDIR)/test_crc.c $(TESTDIR)/test_compat.c
TEST_BINARIES = $(TEST_SOURCES:$(TESTDIR)/%.c=$(BINDIR)/%)

# Example programs  
EXAMPLE_SOURCES = $(SRCDIR)/nlview.c $(SRCDIR)/nllookup.c
EXAMPLE_BINARIES = $(EXAMPLE_SOURCES:$(SRCDIR)/%.c=$(BINDIR)/%)

.PHONY: all clean test examples

all: $(LIBRARY) examples test

# Create directories if they don't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(LIBDIR):
	mkdir -p $(LIBDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Build object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build library
$(LIBRARY): $(OBJECTS) | $(LIBDIR)
	ar rcs $@ $(OBJECTS)

# Build test programs
$(BINDIR)/test_%: $(TESTDIR)/test_%.c $(LIBRARY) | $(BINDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIBDIR) -lftn -o $@

# Build example programs
$(BINDIR)/%: $(SRCDIR)/%.c $(LIBRARY) | $(BINDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIBDIR) -lftn -o $@

examples: $(EXAMPLE_BINARIES)

test: $(TEST_BINARIES)
	@echo "Running tests..."
	@for test in $(TEST_BINARIES); do \
		echo "Running $$test"; \
		$$test || exit 1; \
	done
	@echo "All tests passed!"

clean:
	rm -rf $(OBJDIR) $(LIBDIR) $(BINDIR)

install: $(LIBRARY)
	@echo "Install target not implemented yet"

.SUFFIXES:
.SUFFIXES: .c .o