# Task 07: Daemon Mode and Final Integration

## Objective
Complete the ftntoss implementation with full daemon mode support, comprehensive documentation, and final integration testing. This task finalizes the tosser as a production-ready FTN mail processing system.

## Requirements
1. Full daemon mode implementation with proper signal handling
2. Configuration reload capability (SIGHUP)
3. Comprehensive logging system with syslog integration
4. Example configuration files and documentation
5. Complete integration testing
6. Performance optimization and monitoring
7. Installation and deployment documentation
8. Final code review and cleanup

## Implementation Steps

### 1. Enhanced Daemon Mode Implementation
Enhance `src/ftntoss.c` with complete daemon functionality:

#### Daemon Features
```c
// Daemon mode functions
int daemonize(void);
int setup_daemon_environment(void);
int run_daemon_loop(const ftn_config_t* config, int sleep_interval);
void reload_configuration(void);

// Signal handling for daemon mode
void setup_daemon_signals(void);
void handle_sighup(int sig);
void handle_sigterm(int sig);
void handle_sigusr1(int sig);  // Statistics dump
void handle_sigusr2(int sig);  // Debug mode toggle

// Process management
int write_pid_file(const char* pid_file);
int remove_pid_file(const char* pid_file);
```

#### Daemon Loop Implementation
```c
int run_daemon_loop(const ftn_config_t* config, int sleep_interval) {
    while (!shutdown_requested) {
        // Process all configured networks
        if (process_inbox(config) != 0) {
            log_error("Error processing inbox");
        }

        if (process_outbound(config) != 0) {
            log_error("Error processing outbound");
        }

        // Check for configuration reload
        if (reload_requested) {
            reload_configuration();
            reload_requested = 0;
        }

        // Sleep for configured interval
        for (int i = 0; i < sleep_interval && !shutdown_requested; i++) {
            sleep(1);
        }
    }

    return 0;
}
```

### 2. Comprehensive Logging System

#### Logging Levels and Functions
```c
typedef enum {
    FTN_LOG_DEBUG,
    FTN_LOG_INFO,
    FTN_LOG_WARNING,
    FTN_LOG_ERROR,
    FTN_LOG_CRITICAL
} ftn_log_level_t;

// Logging interface
void ftn_log_init(ftn_log_level_t level, int use_syslog, const char* ident);
void ftn_log_close(void);
void ftn_log(ftn_log_level_t level, const char* format, ...);

// Convenience macros
#define log_debug(fmt, ...) ftn_log(FTN_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) ftn_log(FTN_LOG_INFO, fmt, ##__VA_ARGS__)
#define log_warning(fmt, ...) ftn_log(FTN_LOG_WARNING, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) ftn_log(FTN_LOG_ERROR, fmt, ##__VA_ARGS__)
#define log_critical(fmt, ...) ftn_log(FTN_LOG_CRITICAL, fmt, ##__VA_ARGS__)
```

#### Logging Configuration
Add logging configuration to the INI file:
```ini
[logging]
level = info                    # debug, info, warning, error, critical
use_syslog = yes               # Use syslog for daemon mode
log_file = /var/log/ftntoss.log # Log file for non-daemon mode
ident = ftntoss                # Syslog identifier
```

### 3. Configuration Reload Capability

#### Configuration Management
```c
// Global configuration management
static ftn_config_t* global_config = NULL;
static char* config_file_path = NULL;
static volatile sig_atomic_t reload_requested = 0;

void reload_configuration(void) {
    ftn_config_t* new_config = ftn_config_new();

    if (ftn_config_load(new_config, config_file_path) != FTN_OK) {
        log_error("Failed to reload configuration, keeping current config");
        ftn_config_free(new_config);
        return;
    }

    if (ftn_config_validate(new_config) != FTN_OK) {
        log_error("New configuration is invalid, keeping current config");
        ftn_config_free(new_config);
        return;
    }

    ftn_config_free(global_config);
    global_config = new_config;
    log_info("Configuration reloaded successfully");
}
```

### 4. Statistics and Monitoring

#### Statistics Collection
```c
typedef struct {
    // Counters
    unsigned long packets_processed;
    unsigned long messages_processed;
    unsigned long duplicates_detected;
    unsigned long messages_stored;
    unsigned long messages_forwarded;
    unsigned long errors_total;

    // Timing
    time_t start_time;
    time_t last_cycle_time;
    double avg_cycle_time;

    // Per-network statistics
    ftn_network_stats_t* network_stats;
    size_t network_count;
} ftn_global_stats_t;

// Statistics functions
void ftn_stats_init(void);
void ftn_stats_update(const ftn_processing_stats_t* stats);
void ftn_stats_dump(void);
void ftn_stats_reset(void);
```

#### Statistics Output
Implement SIGUSR1 handler to dump statistics:
```c
void handle_sigusr1(int sig) {
    log_info("=== FTN Tosser Statistics ===");
    ftn_stats_dump();
}
```

### 5. Example Configuration and Documentation

#### Create Example Configuration
Create `examples/ftntoss.ini` with comprehensive example:
```ini
# FTN Tosser Configuration Example
# See ftntoss(1) for detailed documentation

[node]
name = Example Node
networks = fidonet,fsxnet
sysop = John Doe
sysop_name = John Doe
email = sysop@example.org
www = http://example.org
telnet = bbs.example.org

[news]
path = /var/spool/news

[mail]
inbox = /var/mail/%USER%
outbox = /var/mail/%USER%/.Outbox
sent = /var/mail/%USER%/.Sent

[logging]
level = info
use_syslog = yes
ident = ftntoss

[daemon]
pid_file = /var/run/ftntoss.pid
sleep_interval = 60

[fidonet]
name = Fidonet
domain = fidonet.org
address = 1:2/3.4
hub = 1:2/0
inbox = /var/spool/ftn/fidonet/inbox
outbox = /var/spool/ftn/fidonet/outbox
processed = /var/spool/ftn/fidonet/processed
bad = /var/spool/ftn/fidonet/bad
duplicate_db = /var/spool/ftn/fidonet/dupes.db

[fsxnet]
name = fsxNet
domain = fsxnet.nz
address = 21:1/100.0
hub = 21:1/0
inbox = /var/spool/ftn/fsxnet/inbox
outbox = /var/spool/ftn/fsxnet/outbox
processed = /var/spool/ftn/fsxnet/processed
bad = /var/spool/ftn/fsxnet/bad
duplicate_db = /var/spool/ftn/fsxnet/dupes.db
```

#### Create Man Page
Create `docs/ftntoss.1` manual page:
```
.TH FTNTOSS 1 "2025" "libFTN" "FidoNet Tools"
.SH NAME
ftntoss \- FidoNet Technology Network message tosser
.SH SYNOPSIS
.B ftntoss
[\fIOPTION\fR]... \fB\-c\fR \fICONFIG_FILE\fR
.SH DESCRIPTION
ftntoss is a FidoNet message tosser that processes incoming FTN packets and distributes messages to appropriate destinations.
...
```

### 6. Installation and Deployment

#### Update Makefile
Add installation targets:
```makefile
install: $(LIBRARY) $(EXAMPLE_BINARIES)
	@echo "Installing libFTN..."
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include/ftn
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -d $(DESTDIR)$(PREFIX)/share/doc/libftn
	install $(LIBRARY) $(DESTDIR)$(PREFIX)/lib/
	install include/ftn.h $(DESTDIR)$(PREFIX)/include/
	install include/ftn/*.h $(DESTDIR)$(PREFIX)/include/ftn/
	install $(BINDIR)/ftntoss $(DESTDIR)$(PREFIX)/bin/
	install docs/ftntoss.1 $(DESTDIR)$(PREFIX)/share/man/man1/
	install examples/ftntoss.ini $(DESTDIR)$(PREFIX)/share/doc/libftn/
```

#### Create Systemd Service File
Create `examples/ftntoss.service`:
```ini
[Unit]
Description=FidoNet Tosser
After=network.target

[Service]
Type=forking
User=ftn
Group=ftn
ExecStart=/usr/local/bin/ftntoss -c /etc/ftntoss.ini -d
ExecReload=/bin/kill -HUP $MAINPID
PIDFile=/var/run/ftntoss.pid
Restart=always

[Install]
WantedBy=multi-user.target
```

### 7. Final Testing and Validation

#### Create Comprehensive Test Suite
Create `tests/test_final.c` for complete system testing:

#### Test Categories
1. **End-to-end workflow testing**:
   - Complete packet processing scenarios
   - Multi-network configurations
   - Various message types and formats
   - Error recovery scenarios

2. **Daemon mode testing**:
   - Daemon startup and shutdown
   - Signal handling verification
   - Configuration reload testing
   - Long-running stability tests

3. **Performance testing**:
   - High-volume message processing
   - Memory usage validation
   - CPU usage measurement
   - Concurrent operation testing

4. **Integration testing**:
   - Real FTN packet processing
   - Actual file system operations
   - Network configuration validation
   - Error condition handling

#### Create Test Scripts
Create `tests/run_integration_tests.sh`:
```bash
#!/bin/bash
# Comprehensive integration test suite
set -e

echo "Running FTN Tosser Integration Tests"

# Setup test environment
./setup_test_env.sh

# Run unit tests
echo "Running unit tests..."
make test

# Run integration tests
echo "Running integration tests..."
./test_packet_processing.sh
./test_daemon_mode.sh
./test_configuration.sh
./test_performance.sh

# Cleanup
./cleanup_test_env.sh

echo "All tests passed!"
```

### 8. Documentation Updates

#### Update README.md
Add ftntoss documentation to main README:
- Usage examples
- Configuration guide
- Installation instructions
- Troubleshooting guide

#### Create FTNTOSS.md Documentation
Enhance the existing FTNTOSS.md with:
- Complete configuration reference
- Operational procedures
- Troubleshooting guide
- Performance tuning
- Security considerations

#### Update CLAUDE.md
Add ftntoss-specific information for future development:
- Tosser architecture overview
- Testing procedures
- Configuration management
- Debugging techniques

## Testing Requirements

### 1. Final Integration Tests
- Complete system workflow validation
- Multi-network processing verification
- Error handling and recovery testing
- Performance and stability validation

### 2. Daemon Mode Tests
- Proper daemon initialization
- Signal handling verification
- Configuration reload functionality
- Statistics and monitoring

### 3. Documentation Tests
- Example configuration validation
- Installation procedure verification
- Man page accuracy
- Troubleshooting guide effectiveness

## Success Criteria
1. Complete ftntoss implementation with all features
2. Daemon mode functions correctly with proper signal handling
3. Configuration reload works without service interruption
4. Comprehensive logging provides adequate debugging information
5. All integration tests pass with real FTN data
6. Performance meets production requirements
7. Documentation is complete and accurate
8. Installation procedures work on target systems
9. Code quality meets libftn standards
10. Memory usage is bounded and appropriate

## Files to Create/Modify
- `src/ftntoss.c` (final enhancements)
- `examples/ftntoss.ini` (comprehensive example)
- `examples/ftntoss.service` (systemd service file)
- `docs/ftntoss.1` (man page)
- `tests/test_final.c` (comprehensive tests)
- `tests/run_integration_tests.sh` (test runner)
- `tests/setup_test_env.sh` (test environment setup)
- `tests/cleanup_test_env.sh` (test cleanup)
- `README.md` (update with ftntoss information)
- `FTNTOSS.md` (enhance with complete documentation)
- `CLAUDE.md` (update with tosser information)
- `Makefile` (add installation targets)

## Final Deliverables
1. Complete, production-ready ftntoss executable
2. Comprehensive documentation and examples
3. Full test suite with validation scripts
4. Installation and deployment procedures
5. Performance benchmarks and tuning guide
6. Troubleshooting and maintenance documentation

This task completes the FTN tosser implementation as specified in FTNTOSS.md, providing a robust, production-ready mail processing system for FidoNet networks.