
## ftntoss specific information

### Tosser architecture overview
The `ftntoss` utility is a FidoNet message tosser that can run in single-shot or daemon mode. It is configured via an INI file that specifies the node, networks, mail, news, logging, and daemon settings.

### Testing procedures
The `ftntoss` utility has its own test suite in `tests/test_ftntoss.c`. It also has a set of integration test scripts in the `tests` directory (`run_integration_tests.sh`, `setup_test_env.sh`, `cleanup_test_env.sh`).

### Configuration management
The configuration for `ftntoss` is handled by `src/config.c` and defined in `include/ftn/config.h`. The configuration includes sections for `[node]`, `[news]`, `[mail]`, `[logging]`, `[daemon]`, and one section for each network.

### Debugging techniques
The `ftntoss` utility uses the logging library in `src/log.c`. The log level can be set in the configuration file or with the `-v` command-line option. In daemon mode, it responds to `SIGUSR2` to toggle debug logging.
mpatibility layer for non-ANSI/non-POSIX environments

### Key Data Structures

- **`ftn_address_t`** - Four-part FTN addressing (zone:net/node.point)
- **`ftn_message_t`** - Unified message structure supporting both Netmail and Echomail with dynamic control paragraphs
- **`ftn_packet_t`** - Packet container with dynamic message array

### Command-Line Utilities

All utilities are independent programs that link against the core library:

- **`ftntoss`**: A powerful FidoNet message tosser with daemon support.
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