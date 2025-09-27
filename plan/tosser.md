# FTN Tosser Implementation Plan

## Overview

Implement `ftntoss`, a FidoNet tosser that processes incoming FTN packets and distributes messages to appropriate destinations (echomail areas, netmail, USENET newsgroups). The tosser supports both single-shot and continuous operation modes with INI-based configuration.

## Core Requirements Analysis

Based on FTNTOSS.md, the tosser must:

1. **Process FTN packets** - Unpack and sort incoming compressed mail packets
2. **Route messages** - Distribute to local users, forward to other nodes, or place in echomail areas
3. **Detect duplicates** - Prevent message duplication using unique identifiers (MSGID)
4. **Create outbound packets** - Package outgoing messages for transmission
5. **Support two operation modes** - Single-shot (process and exit) or continuous (daemon mode)
6. **Use INI configuration** - Multi-network support with flexible path templating

## Architecture Design

### Core Components

1. **Configuration Manager** (`src/config.c`)
   - Parse INI configuration files with case-insensitive sections/keys
   - Support path templating (%USER%, %NETWORK%)
   - Validate network configurations

2. **Tosser Engine** (`src/ftntoss.c`)
   - Main processing loop for single-shot and continuous modes
   - Packet discovery and processing orchestration
   - Error handling and logging

3. **Message Router** (`src/router.c`)
   - Analyze message addressing and determine routing
   - Duplicate detection using MSGID tracking
   - Route to appropriate destinations (mail, news, forward)

4. **Storage Interface** (`src/storage.c`)
   - Maildir creation and message storage
   - USENET spool format handling with active file management
   - Outbound packet creation

5. **Duplicate Tracker** (`src/dupecheck.c`)
   - MSGID-based duplicate detection
   - Persistent storage of seen message IDs
   - Configurable retention policies

## Implementation Plan

### Phase 1: Core Infrastructure

1. **Create configuration system**
   - Implement INI parser for tosser configuration
   - Add configuration validation and error reporting
   - Support for multiple network sections
   - Path templating engine for %USER% and %NETWORK% substitution

2. **Design main tosser structure**
   - Command-line argument parsing (config file, mode selection)
   - Main processing loop with single-shot and continuous modes
   - Signal handling for graceful shutdown in daemon mode

3. **Add comprehensive unit tests**
   - Test INI parsing with various configurations
   - Test path templating functionality
   - Test command-line argument processing

### Phase 2: Message Processing Core

4. **Implement duplicate detection system**
   - MSGID extraction and normalization
   - Persistent storage using simple database format
   - Efficient lookup and storage operations
   - Age-based cleanup of old duplicate records

5. **Create message routing engine**
   - Analyze FTN message headers and addressing
   - Determine routing decisions (local, forward, echomail)
   - Integration with existing libftn address parsing
   - Support for multiple network configurations

6. **Add routing unit tests**
   - Test MSGID extraction and duplicate detection
   - Test routing decision logic for various message types
   - Test multi-network address resolution

### Phase 3: Storage Integration

7. **Implement maildir storage**
   - Create maildir structure (cur, new, tmp)
   - Convert FTN netmail to RFC822 format using existing rfc822.c
   - Handle user-specific maildir creation
   - Support for %USER% path templating

8. **Implement USENET storage**
   - Create spool directory structure (network/area/article)
   - Convert FTN echomail to RFC1036 format using existing rfc822.c
   - Maintain active file with newsgroup information
   - Handle area name normalization (lowercase conversion)

9. **Add storage unit tests**
   - Test maildir creation and message storage
   - Test USENET spool creation and article storage
   - Test RFC822/RFC1036 conversion integration
   - Test active file management

### Phase 4: Packet Processing

10. **Implement inbox processing**
    - Scan configured inbox directories for FTN packets
    - Process packets using existing packet.c functionality
    - Move processed packets to configured locations
    - Handle malformed packets (move to "bad" folder)

11. **Implement outbound packet creation**
    - Scan mail outbox and news directories for outgoing messages
    - Convert messages back to FTN format using existing functionality
    - Create properly addressed FTN packets
    - Place in appropriate network outbox directories

12. **Add packet processing tests**
    - Test inbox scanning and packet discovery
    - Test packet processing workflow
    - Test outbound packet creation
    - Test error handling for malformed packets

### Phase 5: Integration and Optimization

13. **Add daemon mode support**
    - Implement sleep/wake cycle for continuous operation
    - Configurable polling intervals
    - Signal handling for configuration reload
    - Proper resource cleanup in daemon mode

14. **Implement comprehensive logging**
    - Configurable log levels
    - Detailed processing statistics
    - Error reporting and debugging information
    - Integration with syslog for daemon mode

15. **Performance optimization**
    - Efficient duplicate detection algorithms
    - Batch processing for multiple packets
    - Memory management optimization
    - File I/O optimization for large message volumes

### Phase 6: Testing and Documentation

16. **Create integration tests**
    - End-to-end packet processing scenarios
    - Multi-network configuration testing
    - Daemon mode operation testing
    - Error recovery and resilience testing

17. **Add example configurations**
    - Sample INI files for common setups
    - Multi-network configuration examples
    - Path templating examples
    - Performance tuning guidelines

18. **Update build system**
    - Add ftntoss to Makefile targets
    - Include tosser tests in test suite
    - Create installation targets
    - Update documentation

## File Structure

```
src/
├── ftntoss.c          # Main tosser executable
├── config.c           # INI configuration parser
├── router.c           # Message routing engine
├── storage.c          # Mail/news storage interface
└── dupecheck.c        # Duplicate detection system

include/ftn/
├── config.h           # Configuration structures and API
├── router.h           # Routing function declarations
├── storage.h          # Storage interface declarations
└── dupecheck.h        # Duplicate detection API

tests/
├── test_config.c      # Configuration system tests
├── test_router.c      # Message routing tests
├── test_storage.c     # Storage system tests
├── test_dupecheck.c   # Duplicate detection tests
└── test_ftntoss.c     # Integration tests

examples/
└── ftntoss.ini        # Example configuration file
```

## Integration Points

- **Leverage existing libftn functionality**:
  - packet.c for FTN packet parsing and creation
  - rfc822.c for message format conversion
  - nodelist.c for address validation and routing
  - ftn.c for address parsing and manipulation

- **Build system integration**:
  - Add to Makefile as new utility
  - Include in test suite execution
  - Follow existing naming and build conventions

- **Configuration compatibility**:
  - Use similar INI format as existing pktscan utility
  - Maintain consistency with existing tool interfaces
  - Support standard FTN directory structures

## Success Criteria

1. **Functional requirements met**:
   - Processes FTN packets correctly using existing library functions
   - Routes messages to appropriate destinations
   - Prevents duplicate message processing
   - Supports both operation modes
   - Handles multi-network configurations

2. **Quality standards achieved**:
   - Comprehensive unit test coverage (>90%)
   - Integration tests for key workflows
   - Memory leak detection and prevention
   - Error handling for all failure modes

3. **Performance targets met**:
   - Efficient processing of large packet volumes
   - Minimal memory footprint in daemon mode
   - Fast duplicate detection for high-volume systems
   - Optimized file I/O operations

4. **Documentation complete**:
   - User documentation with configuration examples
   - Developer documentation for API extensions
   - Integration guide for existing FTN setups
   - Troubleshooting and debugging guide