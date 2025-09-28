# Task 5: Production Features and Integration

## Objective
Complete the fnmailer implementation with production-ready features including advanced signal handling, error recovery, performance optimizations, comprehensive logging, and full integration with the existing libftn ecosystem.

## Deliverables

### 1. Advanced Signal Handling
- **Files**: `src/signal_handler.c`, `include/ftn/signal_handler.h`
- **Requirements**:
  - Graceful shutdown on SIGTERM/SIGINT (complete active sessions)
  - Configuration reload on SIGHUP (re-read config, update network settings)
  - Statistics dump on SIGUSR1 (connection stats, transfer rates, errors)
  - Debug toggle on SIGUSR2 (runtime log level changes)
  - Signal safety and proper cleanup

### 2. Comprehensive Error Recovery
- **Files**: `src/error_recovery.c`, `include/ftn/error_recovery.h`
- **Requirements**:
  - Exponential backoff for connection failures
  - Automatic retry logic with configurable limits
  - Session recovery after network interruptions
  - Corrupted file detection and handling
  - Resource leak prevention and cleanup

### 3. Performance Optimizations
- **Files**: `src/performance.c`, `include/ftn/performance.h`
- **Requirements**:
  - Optimal buffer sizes for file transfers
  - Connection pooling and reuse
  - Efficient memory management
  - I/O optimization (sendfile, mmap where available)
  - Bandwidth throttling and QoS support

### 4. Production Logging System
- **Files**: `src/mailer_log.c`, `include/ftn/mailer_log.h`
- **Requirements**:
  - Structured logging with correlation IDs
  - Multiple log levels and categories
  - Session tracking and statistics
  - Performance metrics collection
  - Log rotation and archival support

### 5. Main Executable Implementation
- **Files**: `src/fnmailer_main.c`
- **Requirements**:
  - Complete command-line interface
  - Configuration validation and error reporting
  - Daemon mode initialization
  - Main event loop with proper timing
  - Integration with all subsystems

## Implementation Details

### Signal Handler System
```c
typedef struct {
    volatile sig_atomic_t shutdown_requested;
    volatile sig_atomic_t reload_config;
    volatile sig_atomic_t dump_stats;
    volatile sig_atomic_t toggle_debug;
    int signal_pipe[2];  // Self-pipe for signal handling
} ftn_signal_context_t;

// Signal handling functions
ftn_error_t ftn_signal_init(ftn_signal_context_t* ctx);
ftn_error_t ftn_signal_cleanup(ftn_signal_context_t* ctx);
void ftn_signal_check(ftn_signal_context_t* ctx, ftn_mailer_context_t* mailer);

// Signal handlers
void signal_handler_shutdown(int sig);
void signal_handler_reload(int sig);
void signal_handler_stats(int sig);
void signal_handler_debug(int sig);
```

### Error Recovery Framework
```c
typedef struct {
    int max_retries;
    int base_delay;      // Base delay in seconds
    int max_delay;       // Maximum delay in seconds
    int current_failures;
    time_t last_attempt;
    time_t next_attempt;
} ftn_retry_context_t;

typedef struct {
    char* error_message;
    ftn_error_t error_code;
    time_t timestamp;
    int retry_count;
    ftn_address_t* remote_address;
} ftn_error_record_t;

// Error recovery operations
ftn_error_t ftn_retry_should_attempt(ftn_retry_context_t* ctx, time_t now);
ftn_error_t ftn_retry_record_failure(ftn_retry_context_t* ctx, ftn_error_t error);
ftn_error_t ftn_retry_record_success(ftn_retry_context_t* ctx);
time_t ftn_retry_calculate_delay(int failure_count, int base_delay, int max_delay);
```

### Performance Optimization System
```c
typedef struct {
    size_t optimal_buffer_size;
    int use_sendfile;           // Use sendfile() where available
    int use_mmap;               // Use mmap() for file reading
    int compression_threshold;   // Minimum size for compression
    int max_connections;        // Connection pool size
    int bandwidth_limit;        // Bytes per second limit
} ftn_performance_config_t;

typedef struct {
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t files_sent;
    uint32_t files_received;
    time_t session_start;
    time_t last_activity;
    double transfer_rate;
} ftn_session_stats_t;

// Performance optimization functions
size_t ftn_perf_get_optimal_buffer_size(void);
ftn_error_t ftn_perf_sendfile_transfer(int src_fd, ftn_net_connection_t* conn,
                                     size_t count, size_t* transferred);
ftn_error_t ftn_perf_apply_bandwidth_limit(ftn_session_stats_t* stats,
                                         int limit_bps);
```

### Enhanced Logging System
```c
typedef enum {
    LOG_CATEGORY_GENERAL,
    LOG_CATEGORY_PROTOCOL,
    LOG_CATEGORY_TRANSFER,
    LOG_CATEGORY_AUTH,
    LOG_CATEGORY_ERROR,
    LOG_CATEGORY_STATS
} ftn_log_category_t;

typedef struct {
    char* session_id;           // Unique session identifier
    ftn_address_t* remote_addr; // Remote node address
    time_t start_time;
    ftn_log_category_t category;
    int level;
} ftn_log_context_t;

// Enhanced logging functions
ftn_error_t ftn_log_session_start(ftn_log_context_t* ctx, ftn_address_t* remote);
ftn_error_t ftn_log_session_end(ftn_log_context_t* ctx, ftn_error_t result);
ftn_error_t ftn_log_transfer(ftn_log_context_t* ctx, const char* filename,
                           size_t size, int direction);
ftn_error_t ftn_log_protocol(ftn_log_context_t* ctx, const char* frame_type,
                           const char* details);
```

### Main Application Framework
```c
typedef struct {
    ftn_config_t* config;
    ftn_signal_context_t* signals;
    ftn_network_list_t* networks;
    ftn_session_list_t* active_sessions;
    ftn_performance_config_t* perf_config;
    ftn_log_context_t* log_context;

    int daemon_mode;
    time_t last_config_check;
    ftn_retry_context_t** retry_contexts;
} ftn_mailer_context_t;

// Main application functions
ftn_error_t ftn_mailer_init(ftn_mailer_context_t* ctx, int argc, char* argv[]);
ftn_error_t ftn_mailer_run(ftn_mailer_context_t* ctx);
ftn_error_t ftn_mailer_shutdown(ftn_mailer_context_t* ctx);
```

## Main Event Loop Implementation

### Daemon Mode Operation
```c
ftn_error_t ftn_mailer_daemon_loop(ftn_mailer_context_t* ctx) {
    fd_set read_fds, write_fds;
    struct timeval timeout;
    time_t next_poll_time = time(NULL);

    while (!ctx->signals->shutdown_requested) {
        // Check for signals
        ftn_signal_check(ctx->signals, ctx);

        // Handle configuration reload
        if (ctx->signals->reload_config) {
            ftn_config_reload(ctx->config);
            ctx->signals->reload_config = 0;
        }

        // Handle statistics dump
        if (ctx->signals->dump_stats) {
            ftn_mailer_dump_statistics(ctx);
            ctx->signals->dump_stats = 0;
        }

        // Check if it's time to poll networks
        time_t now = time(NULL);
        if (now >= next_poll_time) {
            ftn_mailer_poll_networks(ctx);
            next_poll_time = ftn_mailer_calculate_next_poll(ctx);
        }

        // Process active sessions
        ftn_mailer_process_sessions(ctx);

        // Wait for activity or timeout
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        int max_fd = ftn_mailer_setup_fdsets(ctx, &read_fds, &write_fds);

        int activity = select(max_fd + 1, &read_fds, &write_fds, NULL, &timeout);
        if (activity > 0) {
            ftn_mailer_handle_activity(ctx, &read_fds, &write_fds);
        }
    }

    return FTN_ERROR_OK;
}
```

### Network Polling Logic
```c
ftn_error_t ftn_mailer_poll_networks(ftn_mailer_context_t* ctx) {
    ftn_network_t* network;
    time_t now = time(NULL);

    for (network = ctx->networks->first; network; network = network->next) {
        // Check if polling is due for this network
        if (now < network->next_poll_time) {
            continue;
        }

        // Check retry context
        if (!ftn_retry_should_attempt(&network->retry_ctx, now)) {
            continue;
        }

        // Check for outbound mail
        if (!ftn_bso_has_outbound(network->outbound_path, network->hub_address)) {
            network->next_poll_time = now + network->poll_interval;
            continue;
        }

        // Initiate connection
        ftn_error_t result = ftn_mailer_connect_to_hub(ctx, network);
        if (result != FTN_ERROR_OK) {
            ftn_retry_record_failure(&network->retry_ctx, result);
            ftn_log_error(ctx->log_context, "Failed to connect to %s: %s",
                         ftn_address_string(network->hub_address),
                         ftn_error_string(result));
        }

        network->next_poll_time = now + network->poll_interval;
    }

    return FTN_ERROR_OK;
}
```

## Statistics and Monitoring

### Statistics Collection
```c
typedef struct {
    // Connection statistics
    uint32_t total_connections;
    uint32_t successful_connections;
    uint32_t failed_connections;

    // Transfer statistics
    uint64_t total_bytes_sent;
    uint64_t total_bytes_received;
    uint32_t total_files_sent;
    uint32_t total_files_received;

    // Protocol statistics
    uint32_t cram_authentications;
    uint32_t compressed_sessions;
    uint32_t crc_verified_files;

    // Error statistics
    uint32_t protocol_errors;
    uint32_t network_errors;
    uint32_t file_errors;

    // Timing statistics
    time_t uptime_start;
    double average_session_time;
    double average_transfer_rate;
} ftn_mailer_statistics_t;

ftn_error_t ftn_mailer_dump_statistics(ftn_mailer_context_t* ctx) {
    ftn_mailer_statistics_t* stats = &ctx->statistics;

    ftn_log_info(ctx->log_context, "=== FNMailer Statistics ===");
    ftn_log_info(ctx->log_context, "Uptime: %ld seconds",
                time(NULL) - stats->uptime_start);
    ftn_log_info(ctx->log_context, "Connections: %u total, %u successful, %u failed",
                stats->total_connections, stats->successful_connections,
                stats->failed_connections);
    ftn_log_info(ctx->log_context, "Data transferred: %llu bytes sent, %llu bytes received",
                stats->total_bytes_sent, stats->total_bytes_received);
    ftn_log_info(ctx->log_context, "Files transferred: %u sent, %u received",
                stats->total_files_sent, stats->total_files_received);
    ftn_log_info(ctx->log_context, "Average transfer rate: %.2f KB/s",
                stats->average_transfer_rate / 1024.0);

    return FTN_ERROR_OK;
}
```

## Configuration File Integration

### Extended Configuration Schema
```ini
[node]
address = 1:234/5
sysop = John Doe
location = Somewhere, USA

[daemon]
pid_file = /var/run/fnmailer.pid
max_connections = 10
poll_interval = 300
log_level = info

[performance]
buffer_size = 8192
use_sendfile = yes
use_compression = yes
compression_threshold = 1024
bandwidth_limit = 0

[fidonet]
hub_address = 1:234/1
hub_hostname = hub.fidonet.org
hub_port = 24554
password = secret123
poll_frequency = 3600
use_cram = yes
use_compression = yes
use_crc = yes
```

## Testing and Validation

### Integration Testing
- Complete daemon startup/shutdown cycle
- Signal handling during active sessions
- Configuration reload with active connections
- Network failure recovery scenarios
- Performance under load

### Stress Testing
- Multiple simultaneous connections
- Large file transfers with interruptions
- High-frequency polling scenarios
- Memory usage under extended operation
- Resource cleanup verification

### Compatibility Testing
- Interoperability with other binkp mailers
- BSO compatibility with existing tools
- Protocol extension negotiation
- Error condition handling

## Dependencies
- All previous tasks (1-4)
- System libraries for signals and process management
- Performance monitoring utilities
- Log rotation tools (logrotate)

## Acceptance Criteria
1. Daemon operates reliably for extended periods
2. All signals are handled gracefully
3. Error recovery works under various failure conditions
4. Performance is optimized for target use cases
5. Logging provides comprehensive operational visibility
6. Statistics accurately reflect system operation
7. Configuration can be updated without service interruption
8. Memory usage remains stable over time
9. Integration with existing libftn tools works correctly
10. Production deployment requirements are met

## Production Deployment Considerations

### Security
- Run with minimal privileges
- Secure file permissions
- Input validation and sanitization
- Resource limit enforcement

### Monitoring
- Health check endpoints
- Log aggregation integration
- Performance metric collection
- Alert thresholds and notifications

### Maintenance
- Automated log rotation
- Configuration backup and versioning
- Update and rollback procedures
- Documentation for operations staff

This final task completes the fnmailer implementation with all necessary production features for reliable operation in a FidoNet environment.