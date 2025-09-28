# Task 1: Core Infrastructure Setup

## Objective
Establish the foundational infrastructure for the fnmailer implementation including configuration management, network abstraction, and basic daemon framework.

## Deliverables

### 1. Enhanced Configuration System
- **File**: `src/config.c`, `include/ftn/config.h`
- **Requirements**:
  - Extend existing config system to support mailer-specific sections
  - Add support for `[daemon]` section with polling intervals
  - Add support for per-network `[network_name]` sections
  - Implement configuration validation for mailer settings
  - Add configuration reload capability for SIGHUP signal

### 2. Network Abstraction Layer
- **Files**: `src/net.c`, `include/ftn/net.h`
- **Requirements**:
  - Cross-platform TCP/IP socket wrapper functions
  - Connection establishment and management
  - Non-blocking I/O support for daemon mode
  - Error handling and timeout management
  - Socket cleanup and resource management
  - Support for both client and server operations

### 3. Basic Daemon Framework
- **Files**: `src/fnmailer.c`, `include/ftn/fnmailer.h`
- **Requirements**:
  - Command-line argument parsing (-c, -d, -v, -h, --version)
  - Signal handler setup (SIGHUP, SIGTERM, SIGINT, SIGUSR1, SIGUSR2)
  - Daemon mode initialization and main event loop
  - Process management (fork, setsid for daemon mode)
  - PID file creation and management

### 4. Enhanced Logging System
- **Files**: `src/log.c`, `include/ftn/log.h`
- **Requirements**:
  - Mailer-specific log categories and levels
  - Session tracking and correlation IDs
  - Performance metrics logging
  - Debug mode toggle via SIGUSR2
  - Log rotation support

## Implementation Details

### Configuration Schema Extensions
```ini
[daemon]
poll_interval = 300
max_connections = 10
pid_file = /var/run/fnmailer.pid

[fidonet]
hub_address = 1:234/5
hub_hostname = hub.fidonet.org
hub_port = 24554
password = secret123
poll_frequency = 3600
compression = yes
cram_auth = yes
```

### Network API Design
```c
// Connection management
ftn_net_connection_t* ftn_net_connect(const char* hostname, int port, int timeout);
int ftn_net_disconnect(ftn_net_connection_t* conn);
int ftn_net_send(ftn_net_connection_t* conn, const void* data, size_t len);
int ftn_net_recv(ftn_net_connection_t* conn, void* buffer, size_t len);

// Server operations
ftn_net_server_t* ftn_net_listen(int port, int max_connections);
ftn_net_connection_t* ftn_net_accept(ftn_net_server_t* server, int timeout);
```

### Signal Handling Strategy
- **SIGHUP**: Reload configuration, re-read network settings
- **SIGTERM/SIGINT**: Graceful shutdown, complete active sessions
- **SIGUSR1**: Dump statistics and status information
- **SIGUSR2**: Toggle debug logging level

## Testing Requirements

### Unit Tests
- Configuration file parsing with various formats
- Network connection establishment and error handling
- Signal handler registration and behavior
- Daemon mode initialization

### Integration Tests
- Full daemon startup and shutdown cycle
- Configuration reload during operation
- Signal handling during active connections

## Dependencies
- Existing libftn configuration system
- Platform-specific networking APIs (BSD sockets/Winsock)
- Signal handling APIs
- File system APIs for PID files and logging

## Acceptance Criteria
1. Configuration system can parse mailer-specific settings
2. Network layer can establish TCP connections reliably
3. Daemon can start, run, and shutdown gracefully
4. All signals are handled appropriately
5. Logging provides adequate detail for debugging
6. Code compiles cleanly on target platforms
7. Basic error conditions are handled properly

## Notes
- Maintain compatibility with existing fntosser configuration
- Follow existing code style and conventions
- Ensure cross-platform compatibility (UNIX-like systems)
- Use defensive programming practices for network operations