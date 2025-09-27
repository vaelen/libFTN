# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build everything (library, utilities, and tests)
make

# Build only the library
make lib/libftn.a

# Build only utilities
make examples

# Run all tests
make test

# Run a specific test
./bin/tests/test_rfc822

# Clean build artifacts
make clean
```

## Architecture Overview

libFTN is a portable ANSI C (C89) library for FidoNet Technology Network (FTN) protocols with a layered architecture:

### Core Library Structure

- **`src/packet.c`** (1,032 lines) - Main packet parsing engine handling FTN binary formats and message structures
- **`src/rfc822.c`** (1,358 lines) - Bidirectional conversion system between FTN and Internet message formats (RFC822/RFC1036)
- **`src/nodelist.c`** (590 lines) - FidoNet nodelist parsing and network routing functionality
- **`src/ftn.c`** - Core utilities including FTN address parsing and manipulation
- **`src/compat.c`** - Compatibility layer for non-ANSI/non-POSIX environments

### Key Data Structures

- **`ftn_address_t`** - Four-part FTN addressing (zone:net/node.point)
- **`ftn_message_t`** - Unified message structure supporting both Netmail and Echomail with dynamic control paragraphs
- **`ftn_packet_t`** - Packet container with dynamic message array

### Command-Line Utilities

All utilities are independent programs that link against the core library:

- **Conversion tools**: `pkt2mail`, `pkt2news`, `msg2pkt` - Format conversion between FTN and Internet standards
- **Packet tools**: `pktview`, `pktlist`, `pktcreate`, `pktbundle` - Packet inspection and manipulation
- **Nodelist tools**: `nlview`, `nllookup` - Network directory operations

### Conversion System Design

The conversion system provides bidirectional translation with message fidelity preservation:

1. **Address mapping**: FTN addresses (zone:net/node.point) ↔ RFC822 format (node.net.zone.point@domain)
2. **Message types**: Netmail → email, Echomail → USENET articles
3. **Control paragraphs**: FTS standards (MSGID, REPLY, INTL, TZUTC, Via) mapped to RFC822 headers
4. **USENET integration**: Areas become newsgroups with `network.area` naming and active file management

## Development Patterns

### Error Handling
All library functions return `ftn_error_t` enum values. Check return codes and handle specific error conditions (FTN_ERROR_NOMEM, FTN_ERROR_PARSE, etc.).

### Memory Management
- Use `_new()` and `_free()` function pairs consistently
- Dynamic arrays grow automatically (packet messages, message headers)
- Always call cleanup functions to prevent leaks

### Standards Compliance
- Implements FTS (FidoNet Technical Standards) specifications precisely
- Maintains RFC822/RFC1036 compliance for Internet interoperability
- Preserves backward compatibility with legacy FTN systems

## Testing Strategy

Tests are comprehensive with roundtrip validation:
- **`test_rfc822`** - Full bidirectional conversion testing (13/13 tests)
- **`test_packet`** - Binary packet parsing and generation
- **`test_control_paragraphs`** - FTS control line handling
- Individual tests for each core module

Run specific test categories or use `make test` for full validation before committing changes.