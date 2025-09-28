# Task 3: BSO File Management and Transfer System

## Objective
Implement BinkleyTerm Style Outbound (BSO) file management system and core file transfer capabilities for the binkp protocol as specified in FTS-5005.

## Deliverables

### 1. BSO Directory Structure Management
- **Files**: `src/bso.c`, `include/ftn/bso.h`
- **Requirements**:
  - Outbound directory scanning and management
  - Zone-specific directory handling (.001, .002, etc.)
  - Point system support (.pnt subdirectories)
  - Domain addressing support for multiple FTNs
  - Flow file naming convention (8-hex-digit format)

### 2. Flow File Processing
- **Files**: `src/flow.c`, `include/ftn/flow.h`
- **Requirements**:
  - Flow file type detection (netmail .?ut, reference .?lo)
  - Flow file flavor processing (immediate, continuous, direct, normal, hold)
  - Reference file parsing with directives (#, ^, ~, @, -, !)
  - Netmail packet handling and dynamic renaming
  - File path resolution and validation

### 3. Control File Management
- **Files**: `src/control.c`, `include/ftn/control.h`
- **Requirements**:
  - .bsy (busy) file locking mechanism
  - .csy (call) file coordination
  - .hld (hold) file time-based holds
  - .try (try) file connection attempt tracking
  - Race condition prevention in file creation
  - Age-based cleanup of stale control files

### 4. File Transfer Engine
- **Files**: `src/transfer.c`, `include/ftn/transfer.h`
- **Requirements**:
  - File transmission with metadata (M_FILE command)
  - File reception and validation
  - Resume capability (offset-based transfers)
  - Acknowledgment handling (M_GOT, M_GET, M_SKIP)
  - Batch processing (M_EOB handling)
  - Temporary file management

## Implementation Details

### BSO Directory Structure
```c
typedef struct {
    char* base_path;        // Base outbound directory
    char* domain;           // Domain name (NULL for default)
    int zone;               // Zone number
    ftn_address_t* address; // Target address
} ftn_bso_path_t;

// BSO path operations
char* ftn_bso_get_outbound_path(ftn_bso_path_t* bso_path);
char* ftn_bso_get_flow_filename(ftn_address_t* addr, const char* flavor, const char* type);
ftn_error_t ftn_bso_scan_outbound(const char* outbound_path, ftn_flow_list_t** flows);
```

### Flow File System
```c
typedef enum {
    FLOW_TYPE_NETMAIL,    // .?ut files
    FLOW_TYPE_REFERENCE   // .?lo files
} ftn_flow_type_t;

typedef enum {
    FLOW_FLAVOR_IMMEDIATE,  // i??
    FLOW_FLAVOR_CONTINUOUS, // c??
    FLOW_FLAVOR_DIRECT,     // d??
    FLOW_FLAVOR_NORMAL,     // out/flo
    FLOW_FLAVOR_HOLD        // h??
} ftn_flow_flavor_t;

typedef struct {
    char* filepath;
    ftn_flow_type_t type;
    ftn_flow_flavor_t flavor;
    ftn_address_t* target_address;
    time_t timestamp;
    size_t file_count;      // For reference files
} ftn_flow_file_t;
```

### Reference File Entry Processing
```c
typedef enum {
    REF_DIRECTIVE_NONE,     // No directive - send only
    REF_DIRECTIVE_TRUNCATE, // # - truncate after send
    REF_DIRECTIVE_DELETE,   // ^ or - - delete after send
    REF_DIRECTIVE_SKIP,     // ~ or ! - skip processing
    REF_DIRECTIVE_SEND      // @ - explicit send directive
} ftn_ref_directive_t;

typedef struct {
    char* filepath;
    ftn_ref_directive_t directive;
    int processed;
} ftn_reference_entry_t;
```

### Control File Management
```c
typedef struct {
    ftn_address_t* address;
    char* control_path;
    time_t created;
    char* pid_info;         // Optional PID information
} ftn_control_file_t;

// Control file operations
ftn_error_t ftn_control_acquire_bsy(ftn_address_t* addr, const char* outbound);
ftn_error_t ftn_control_release_bsy(ftn_address_t* addr, const char* outbound);
ftn_error_t ftn_control_check_hold(ftn_address_t* addr, const char* outbound, time_t* until);
ftn_error_t ftn_control_create_call(ftn_address_t* addr, const char* outbound);
```

### File Transfer Context
```c
typedef struct {
    char* filename;
    size_t total_size;
    size_t transferred;
    time_t timestamp;
    FILE* file_handle;
    ftn_ref_directive_t action;
    int is_netmail;
} ftn_file_transfer_t;

typedef struct {
    ftn_file_transfer_t** pending_files;
    size_t pending_count;
    ftn_file_transfer_t* current_send;
    ftn_file_transfer_t* current_recv;
} ftn_transfer_context_t;
```

## BSO Processing Logic

### Flow File Priority Order
1. Immediate (.i??) - highest priority, ignore restrictions
2. Continuous (.c??) - honor internal restrictions only
3. Direct (.d??) - honor all restrictions
4. Normal (.out/.flo) - standard processing
5. Hold (.h??) - wait for remote poll

### Reference File Directive Handling
```c
// Process reference file entry
ftn_error_t process_reference_entry(const char* line, ftn_reference_entry_t* entry) {
    if (line[0] == '#') {
        entry->directive = REF_DIRECTIVE_TRUNCATE;
        entry->filepath = strdup(line + 1);
    } else if (line[0] == '^' || line[0] == '-') {
        entry->directive = REF_DIRECTIVE_DELETE;
        entry->filepath = strdup(line + 1);
    } else if (line[0] == '~' || line[0] == '!') {
        entry->directive = REF_DIRECTIVE_SKIP;
        entry->filepath = strdup(line + 1);
    } else if (line[0] == '@') {
        entry->directive = REF_DIRECTIVE_SEND;
        entry->filepath = strdup(line + 1);
    } else {
        entry->directive = REF_DIRECTIVE_NONE;
        entry->filepath = strdup(line);
    }
    return FTN_ERROR_OK;
}
```

### Control File Locking Strategy
```c
// Safe .bsy file creation with race condition prevention
ftn_error_t create_bsy_file(const char* bsy_path) {
    int fd = open(bsy_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd == -1) {
        if (errno == EEXIST) {
            return FTN_ERROR_BUSY;  // Already locked
        }
        return FTN_ERROR_FILE_IO;
    }

    // Write PID information
    char pid_info[64];
    snprintf(pid_info, sizeof(pid_info), "fnmailer %d\n", getpid());
    write(fd, pid_info, strlen(pid_info));
    close(fd);

    return FTN_ERROR_OK;
}
```

## File Transfer Protocol Integration

### Outbound File Processing
1. Scan outbound directories for flow files
2. Sort by flavor priority (immediate → hold)
3. Acquire .bsy lock for target address
4. Process netmail packets (.?ut files)
5. Process reference files (.?lo files)
6. Build file transmission queue
7. Release locks after completion

### File Transmission Sequence
1. Send M_FILE with metadata
2. Stream file data in chunks
3. Wait for acknowledgment (M_GOT/M_GET/M_SKIP)
4. Apply post-transmission action (delete/truncate)
5. Update flow files and control files

### File Reception Handling
1. Receive M_FILE command
2. Validate filename and create temporary file
3. Receive data frames and write to file
4. Verify file completion
5. Send appropriate acknowledgment
6. Move to final destination

## Error Handling and Recovery

### File System Errors
- Permission denied → log error, skip file
- Disk full → pause transmission, retry later
- File not found → remove from flow file, continue
- Corruption detected → request retransmission

### Control File Issues
- Stale .bsy files → age-based cleanup on startup
- Race conditions → proper atomic file operations
- Lock conflicts → appropriate backoff and retry

## Testing Requirements

### Unit Tests
- BSO directory structure parsing
- Flow file flavor detection and priority
- Reference file directive parsing
- Control file locking mechanisms
- File path resolution and validation

### Integration Tests
- Complete outbound processing cycle
- File transfer with various sizes and types
- Control file coordination between processes
- Error recovery scenarios

### Stress Tests
- Large numbers of pending files
- Concurrent access to outbound directories
- Long-running transfer sessions
- Network interruption recovery

## Dependencies
- Binkp protocol implementation from Task 2
- File system APIs for directory scanning
- Time functions for timestamp handling
- Process APIs for PID management

## Acceptance Criteria
1. Can scan and process BSO directory structures
2. Correctly prioritizes flow files by flavor
3. Handles all reference file directives properly
4. Implements safe control file locking
5. Manages file transfers reliably
6. Recovers gracefully from errors
7. Maintains BSO compatibility with existing tools
8. Provides comprehensive logging for debugging

## BSO Compatibility Notes
- Strict adherence to FTS-5005 specification
- Case-insensitive filename handling where appropriate
- Support for both 8.3 and long filename systems
- Proper handling of point system addressing
- Domain-aware outbound management

## Future Enhancement Points
- Compression support for transmitted files
- Advanced file queuing and prioritization
- Bandwidth throttling capabilities
- Statistics collection and reporting