# FNMailer Implementation Plan

## Overview

The FNMailer (FidoNet Mailer) is a TCP/IP-based mailer implementation that uses the binkp protocol to communicate with other FidoNet nodes for sending and receiving FTN packets. This document outlines the complete implementation plan for building the fnmailer service as described in FNMAILER.md.

## System Architecture

### Core Components

1. **Configuration Management** (`src/config.c`, `include/ftn/config.h`)
   - INI file parser for mailer configuration
   - Support for node, network, mail, news, logging, and daemon sections
   - Shared configuration system with fntosser

2. **Network Library** (`src/net.c`, `include/ftn/net.h`)
   - Cross-platform TCP/IP socket abstraction
   - Connection management and error handling
   - Platform-specific networking implementations

3. **Binkp Protocol Implementation** (`src/binkp.c`, `include/ftn/binkp.h`)
   - Complete binkp/1.0 protocol state machine
   - Frame parsing and generation
   - Session management (authentication, file transfer, termination)
   - Support for protocol extensions (CRAM, NR, PLZ, CRC)

4. **File Management System**
   - BSO (BinkleyTerm Style Outbound) flow files support
   - Packet compression/decompression
   - File transfer reliability and resume capability

5. **Daemon Infrastructure**
   - Signal handling (SIGHUP, SIGTERM, SIGINT, SIGUSR1, SIGUSR2)
   - Process management and logging
   - Timer-based polling logic

## Protocol Implementation Details

### Binkp/1.0 Core Protocol (FTS-1026)

**Session Setup Stage:**
- M_NUL frames for system information exchange
- M_ADR frames for address presentation
- M_PWD/M_OK authentication handshake
- Protocol version negotiation

**File Transfer Stage:**
- M_FILE frames with file metadata
- Data frames for file content
- M_GOT/M_GET/M_SKIP acknowledgment system
- M_EOB end-of-batch signaling
- Bidirectional simultaneous transfer

**Error Handling:**
- M_ERR for fatal errors
- M_BSY for non-fatal busy conditions
- Graceful session termination

### Protocol Extensions

**CRAM Authentication (FTS-1027):**
- Challenge-response authentication
- MD5 and SHA1 hash function support
- Keyed hashing with HMAC
- Backward compatibility with plain passwords

**Non-Reliable Mode (FTS-1028):**
- Offset-based file resume capability
- M_GET with explicit offset requests
- Performance optimization for unreliable connections

**Dataframe Compression (FTS-1029):**
- ZLib compression for data frames
- PLZ mode negotiation
- Dynamic compression control

**CRC Checksum (FTS-1030):**
- CRC32 file integrity verification
- Extended M_FILE with checksum parameter
- Error detection and recovery

### BSO Flow File System (FTS-5005)

**Flow File Types:**
- Netmail packets (.?ut extensions)
- File reference lists (.?lo extensions)

**Flow File Flavors:**
- Immediate (i??) - ignore all restrictions
- Continuous (c??) - honor internal restrictions only
- Direct (d??) - honor all restrictions
- Normal (out/flo) - normal scheduling
- Hold (h??) - wait for remote poll

**Control Files:**
- .bsy (busy) - session locking
- .csy (call) - call coordination
- .hld (hold) - temporary holds
- .try (try) - connection attempt tracking

## Operation Modes

### Single-Shot Mode
1. Read configuration file
2. Process all configured networks sequentially
3. Connect to each hub, exchange files
4. Exit after completing all transfers

### Daemon Mode
1. Read configuration file
2. Initialize polling timers for each network
3. Main event loop:
   - Check timer expiration for each network
   - Initiate connections as needed
   - Update connection timestamps
   - Sleep and repeat
4. Handle signals for configuration reload and shutdown

## Configuration System

### Sections
- `[node]` - Local node information
- `[daemon]` - Daemon-specific settings
- `[logging]` - Log configuration
- `[mail]` - Mail handling settings
- `[news]` - News/echomail settings
- `[network_name]` - Per-network hub configuration

### Network Configuration
- Hub address and connection details
- Authentication credentials
- Polling intervals
- Protocol options and capabilities

## File Processing Pipeline

### Outbound Processing
1. Scan BSO outbound directories
2. Process flow files by priority (immediate → continuous → direct → normal → hold)
3. Create file lists for transmission
4. Handle compression and archiving

### Inbound Processing
1. Receive files via binkp protocol
2. Store in temporary locations
3. Verify integrity (CRC if enabled)
4. Move to appropriate inbound directories
5. Trigger processing by tosser

## Security Considerations

### Authentication
- Support both plain-text and CRAM authentication
- Secure password storage and handling
- Address-based access control

### File Safety
- Validate file names and paths
- Prevent directory traversal attacks
- Secure temporary file handling

## Error Handling and Recovery

### Connection Failures
- Automatic retry with exponential backoff
- Temporary hold file creation for persistent failures
- Detailed logging of failure conditions

### Protocol Errors
- Graceful handling of malformed frames
- Session recovery capabilities
- Data integrity verification

## Testing Strategy

### Unit Tests
- Individual protocol frame parsing
- State machine transitions
- Configuration file parsing
- Network abstraction layer

### Integration Tests
- Full binkp session simulation
- Multi-network scenarios
- Error condition testing
- Performance benchmarking

### Compatibility Tests
- Interoperability with existing mailers
- Protocol extension negotiation
- BSO compatibility verification

## Implementation Phases

### Phase 1: Core Infrastructure
- Configuration system
- Network abstraction layer
- Basic logging and daemon framework

### Phase 2: Binkp Protocol Core
- Frame parsing and generation
- Basic session state machine
- Authentication (plain-text)

### Phase 3: File Transfer
- BSO flow file processing
- File transmission and reception
- Basic error handling

### Phase 4: Protocol Extensions
- CRAM authentication
- Non-reliable mode
- Compression support
- CRC verification

### Phase 5: Production Features
- Signal handling
- Advanced error recovery
- Performance optimizations
- Comprehensive logging

## Dependencies

### External Libraries
- Standard C library (ISO C89)
- Platform-specific networking APIs
- ZLib for compression (optional)
- Crypto libraries for CRAM (optional)

### Internal Dependencies
- libftn core library components
- Configuration management system
- Logging infrastructure

## Performance Considerations

### Memory Management
- Efficient buffer management for file transfers
- Minimal memory allocation during transfers
- Proper cleanup and leak prevention

### Network Efficiency
- Pipelined file transfers
- Optimal buffer sizes
- Connection reuse where possible

### Scalability
- Support for multiple simultaneous connections
- Efficient polling algorithms
- Resource limit management

## Monitoring and Diagnostics

### Logging
- Structured logging with multiple levels
- Session tracking and statistics
- Error reporting and analysis

### Statistics
- Transfer rates and volumes
- Connection success/failure rates
- Protocol extension usage

### Debugging
- Verbose protocol tracing
- Frame-level debugging
- State machine visualization

This implementation plan provides a comprehensive roadmap for building a fully-featured FidoNet mailer that implements the binkp protocol with all standard extensions while maintaining compatibility with existing FTN infrastructure.