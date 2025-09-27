# Task 02: Main Tosser Structure and Command-Line Interface

## Objective
Create the main `ftntoss` executable with command-line argument parsing, operation mode support (single-shot and continuous), and basic application framework.

## Requirements
1. Command-line argument parsing for configuration file and operation mode
2. Single-shot mode: process inbox once and exit
3. Continuous mode: daemon-like operation with configurable sleep intervals
4. Signal handling for graceful shutdown
5. Basic logging framework
6. Integration with Task 01 configuration system

## Implementation Steps

### 1. Create Main Executable Source
Create `src/ftntoss.c` with:
- Main function and command-line argument parsing
- Operation mode selection and execution
- Signal handling setup
- Basic error handling and logging

### 2. Core Functions to Implement
```c
// Main application functions
int main(int argc, char* argv[]);
void print_usage(const char* program_name);
void print_version(void);

// Operation mode functions
int run_single_shot(const ftn_config_t* config);
int run_continuous(const ftn_config_t* config, int sleep_interval);

// Signal handling
void setup_signal_handlers(void);
void signal_handler(int sig);

// Logging functions
void log_info(const char* format, ...);
void log_error(const char* format, ...);
void log_debug(const char* format, ...);
```

### 3. Command-Line Arguments
Support the following command-line options:
- `-c, --config FILE` - Configuration file path (required)
- `-d, --daemon` - Run in continuous (daemon) mode
- `-s, --sleep SECONDS` - Sleep interval for daemon mode (default: 60)
- `-v, --verbose` - Enable verbose logging
- `-h, --help` - Show help message
- `--version` - Show version information

### 4. Application Flow
```
1. Parse command-line arguments
2. Load and validate configuration
3. Setup signal handlers
4. Initialize logging
5. Choose operation mode:
   - Single-shot: Process inbox once, then exit
   - Continuous: Loop with sleep intervals
6. Cleanup and exit
```

### 5. Basic Processing Framework
Create placeholder functions for packet processing that will be implemented in later tasks:
```c
// Placeholder functions (to be implemented in later tasks)
int process_inbox(const ftn_config_t* config);
int process_network_inbox(const ftn_network_config_t* network);
```

### 6. Signal Handling
- `SIGTERM` and `SIGINT`: Graceful shutdown
- `SIGHUP`: Reload configuration (daemon mode only)
- `SIGPIPE`: Ignore (for robustness)

## Testing Requirements

### 1. Create Test File
Create `tests/test_ftntoss.c` with integration-style tests:

### 2. Test Cases to Implement
1. **Command-line parsing**:
   - Valid argument combinations
   - Invalid arguments
   - Help and version output
   - Default values

2. **Configuration integration**:
   - Load valid configuration
   - Handle missing configuration file
   - Handle invalid configuration
   - Configuration file path resolution

3. **Operation mode selection**:
   - Single-shot mode execution
   - Continuous mode setup (without actual daemon loop)
   - Sleep interval validation
   - Signal handling setup

4. **Error handling**:
   - Invalid command-line arguments
   - Missing configuration file
   - Permission errors
   - Signal handling verification

5. **Logging functionality**:
   - Log level filtering
   - Message formatting
   - Output destination verification

### 3. Mock Implementation
For testing purposes, implement mock versions of processing functions:
```c
// Mock functions for testing
int mock_process_inbox(const ftn_config_t* config);
int mock_process_network_inbox(const ftn_network_config_t* network);
```

### 4. Test Configuration Files
Create test configuration files in `tests/data/`:
- `ftntoss_test.ini` - Valid test configuration
- Use existing configuration files from Task 01

## Build System Integration

### 1. Update Makefile
Add ftntoss executable to the build system:
- Add to EXAMPLE_SOURCES and EXAMPLE_BINARIES
- Ensure proper linking with libftn.a
- Include in main build targets

### 2. Dependencies
The ftntoss executable depends on:
- libftn.a (core library)
- Configuration system from Task 01
- Standard C libraries (signal.h, unistd.h, etc.)

## Implementation Notes

### 1. ANSI C Compliance
- Use only ANSI C (C89) features
- Avoid POSIX-specific features where possible
- Use existing compat.c functionality for portability

### 2. Memory Management
- Proper cleanup of configuration structures
- Handle memory allocation failures gracefully
- No memory leaks in normal or error conditions

### 3. Error Reporting
- Clear, actionable error messages
- Consistent error codes
- Proper exit status codes for shell scripting

### 4. Logging Design
- Simple but effective logging system
- Support for different log levels
- Optional syslog integration for daemon mode

## Success Criteria
1. `ftntoss` executable builds successfully
2. All command-line argument parsing tests pass
3. Configuration loading integration works correctly
4. Signal handling functions properly
5. Both operation modes can be invoked (even with placeholder processing)
6. Memory management is leak-free
7. Error handling provides clear feedback
8. Code follows existing libftn conventions

## Files to Create/Modify
- `src/ftntoss.c` (new)
- `tests/test_ftntoss.c` (new)
- `tests/data/ftntoss_test.ini` (new)
- `Makefile` (modify to include ftntoss executable)

## Future Task Integration
This task creates the framework that will be extended by:
- Task 03: Duplicate detection system
- Task 04: Message routing engine
- Task 05: Storage implementation
- Task 06: Packet processing implementation
- Task 07: Full daemon mode functionality