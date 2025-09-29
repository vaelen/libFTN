# libFTN Makefile
# Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>

CC = cc
CFLAGS = -Wall -Wextra -pedantic -std=c89 -O2
INCLUDES = -Iinclude -Ideps/zlib
LIBDIR = lib
BINDIR = bin
OBJDIR = obj
SRCDIR = src
TESTDIR = tests

# Library name
LIBRARY = $(LIBDIR)/libftn.a

# Zlib library
ZLIB_LIB = deps/zlib/libz.a

# Source files
SOURCES = $(SRCDIR)/ftn.c $(SRCDIR)/crc.c $(SRCDIR)/nodelist.c $(SRCDIR)/search.c $(SRCDIR)/compat.c $(SRCDIR)/packet.c $(SRCDIR)/rfc822.c $(SRCDIR)/version.c $(SRCDIR)/config.c $(SRCDIR)/dupechk.c $(SRCDIR)/router.c $(SRCDIR)/storage.c $(SRCDIR)/log.c $(SRCDIR)/net.c $(SRCDIR)/mailer.c $(SRCDIR)/binkp.c $(SRCDIR)/binkp/commands.c $(SRCDIR)/binkp/session.c $(SRCDIR)/binkp/auth.c $(SRCDIR)/bso.c $(SRCDIR)/flow.c $(SRCDIR)/control.c $(SRCDIR)/transfer.c $(SRCDIR)/binkp/cram.c $(SRCDIR)/binkp/nr.c $(SRCDIR)/binkp/plz.c $(SRCDIR)/binkp/crc.c
OBJECTS = $(SRCDIR)/ftn.o $(SRCDIR)/crc.o $(SRCDIR)/nodelist.o $(SRCDIR)/search.o $(SRCDIR)/compat.o $(SRCDIR)/packet.o $(SRCDIR)/rfc822.o $(SRCDIR)/version.o $(SRCDIR)/config.o $(SRCDIR)/dupechk.o $(SRCDIR)/router.o $(SRCDIR)/storage.o $(SRCDIR)/log.o $(SRCDIR)/net.o $(SRCDIR)/mailer.o $(SRCDIR)/binkp.o $(SRCDIR)/binkp/commands.o $(SRCDIR)/binkp/session.o $(SRCDIR)/binkp/auth.o $(SRCDIR)/bso.o $(SRCDIR)/flow.o $(SRCDIR)/control.o $(SRCDIR)/transfer.o $(SRCDIR)/binkp/cram.o $(SRCDIR)/binkp/nr.o $(SRCDIR)/binkp/plz.o $(SRCDIR)/binkp/crc.o
OBJECTS := $(addprefix $(OBJDIR)/,$(OBJECTS:$(SRCDIR)/%=%))

# Test programs
TEST_SOURCES = $(TESTDIR)/nodelist.c $(TESTDIR)/crc.c $(TESTDIR)/compat.c $(TESTDIR)/packet.c $(TESTDIR)/ctrlpar.c $(TESTDIR)/rfc822.c $(TESTDIR)/config.c $(TESTDIR)/fntosser.c $(TESTDIR)/dupechk.c $(TESTDIR)/router.c $(TESTDIR)/storage.c $(TESTDIR)/integrat.c $(TESTDIR)/plz.c $(TESTDIR)/final.c
TEST_BINARIES = $(TEST_SOURCES:$(TESTDIR)/%.c=$(BINDIR)/tests/%)

# Example programs
EXAMPLE_SOURCES = $(SRCDIR)/nlview.c $(SRCDIR)/nllookup.c $(SRCDIR)/pktlist.c $(SRCDIR)/pktview.c $(SRCDIR)/pktnew.c $(SRCDIR)/pktjoin.c $(SRCDIR)/pkt2mail.c $(SRCDIR)/msg2pkt.c $(SRCDIR)/pkt2news.c $(SRCDIR)/pktscan.c $(SRCDIR)/fntosser.c $(SRCDIR)/fnmailer.c
EXAMPLE_BINARIES = $(EXAMPLE_SOURCES:$(SRCDIR)/%.c=$(BINDIR)/%)

.PHONY: all clean test examples zlib

all: $(LIBRARY) examples test

# Create directories if they don't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/binkp:
	mkdir -p $(OBJDIR)/binkp

$(LIBDIR):
	mkdir -p $(LIBDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/tests:
	mkdir -p $(BINDIR)/tests

# Build zlib library
$(ZLIB_LIB):
	cd deps/zlib && ./configure --static && $(MAKE)

# Build object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build library
$(LIBRARY): $(OBJECTS) | $(LIBDIR)
	ar rcs $@ $(OBJECTS)

# Build test programs
$(BINDIR)/tests/%: $(TESTDIR)/%.c $(LIBRARY) $(ZLIB_LIB) | $(BINDIR)/tests
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIBDIR) -lftn $(ZLIB_LIB) -o $@

# Build example programs (fnmailer needs zlib)
$(BINDIR)/fnmailer_main: $(SRCDIR)/fnmailer_main.c $(LIBRARY) $(ZLIB_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIBDIR) -lftn $(ZLIB_LIB) -o $@
	ln -sf fnmailer_main $(BINDIR)/fnmailer

# Build other example programs
$(BINDIR)/%: $(SRCDIR)/%.c $(LIBRARY) | $(BINDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIBDIR) -lftn -o $@

examples: $(EXAMPLE_BINARIES)

test: examples $(TEST_BINARIES)
	@echo "Running tests..."
	@for test in $(TEST_BINARIES); do \
		echo "Running $$test"; \
		$$test || exit 1; \
	done
	@echo "All tests passed!"

clean:
	rm -rf $(OBJDIR) $(LIBDIR) $(BINDIR) tmp/*
	cd deps/zlib && $(MAKE) clean || true

install: all
	@echo "Installing libFTN to $(DESTDIR)$(PREFIX)..."
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include/ftn
	install -d $(DESTDIR)$(PREFIX)/include/ftn/binkp
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -d $(DESTDIR)$(PREFIX)/share/doc/libftn
	install -m 644 $(LIBRARY) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 include/ftn.h $(DESTDIR)$(PREFIX)/include/
	install -m 644 include/ftn/*.h $(DESTDIR)$(PREFIX)/include/ftn/
	install -m 644 include/ftn/binkp/*.h $(DESTDIR)$(PREFIX)/include/ftn/binkp/
	install -m 755 $(EXAMPLE_BINARIES) $(DESTDIR)$(PREFIX)/bin/
	install -m 644 docs/fntosser.1 $(DESTDIR)$(PREFIX)/share/man/man1/
	install -m 644 examples/fntosser.ini $(DESTDIR)$(PREFIX)/share/doc/libftn/
	@echo "Installation complete."

.SUFFIXES:
.SUFFIXES: .c .o
