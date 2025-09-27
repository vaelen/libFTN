# Task 05: Storage Implementation

## Objective
Implement storage interfaces for maildir (netmail) and USENET spool (echomail) formats, integrating with existing libftn RFC822/RFC1036 conversion functionality.

## Requirements
1. Maildir format implementation for netmail storage
2. USENET spool format implementation for echomail storage
3. Integration with existing rfc822.c conversion functions
4. Support for path templating (%USER%, %NETWORK%)
5. Atomic operations for reliable message storage
6. Proper file permissions and directory creation
7. Active file management for newsgroups

## Implementation Steps

### 1. Create Header File
Create `include/ftn/storage.h` with:
- Storage interface structures and function declarations
- Maildir and news storage configuration
- Error codes for storage operations

### 2. Create Implementation File
Create `src/storage.c` with:
- Maildir creation and message storage functions
- USENET spool directory management
- Integration with existing RFC822/RFC1036 conversion
- Atomic file operations

### 3. Core Functions to Implement
```c
// Storage system lifecycle
ftn_storage_t* ftn_storage_new(const ftn_config_t* config);
void ftn_storage_free(ftn_storage_t* storage);

// Maildir operations
ftn_error_t ftn_storage_store_mail(ftn_storage_t* storage, const ftn_message_t* msg, const char* username, const char* network);
ftn_error_t ftn_storage_create_maildir(const char* path);
ftn_error_t ftn_storage_generate_maildir_filename(char* buffer, size_t size);

// USENET spool operations
ftn_error_t ftn_storage_store_news(ftn_storage_t* storage, const ftn_message_t* msg, const char* area, const char* network);
ftn_error_t ftn_storage_create_newsgroup(ftn_storage_t* storage, const char* newsgroup);
ftn_error_t ftn_storage_update_active_file(ftn_storage_t* storage, const char* newsgroup, long article_num);

// Outbound message scanning
ftn_error_t ftn_storage_scan_outbound_mail(ftn_storage_t* storage, const char* username, ftn_message_list_t* messages);
ftn_error_t ftn_storage_scan_outbound_news(ftn_storage_t* storage, const char* area, ftn_message_list_t* messages);

// Utility functions
char* ftn_storage_expand_path(const char* template, const char* username, const char* network);
ftn_error_t ftn_storage_ensure_directory(const char* path, mode_t mode);
```

### 4. Data Structures
```c
typedef struct {
    const ftn_config_t* config;
    char* news_root;             // Base news spool directory
    char* mail_root;             // Base mail directory
    FILE* active_file;           // Active file handle
} ftn_storage_t;

typedef struct {
    ftn_message_t** messages;    // Array of message pointers
    size_t count;                // Number of messages
    size_t capacity;             // Allocated capacity
} ftn_message_list_t;

typedef struct {
    char* newsgroup;             // Newsgroup name
    long first_article;          // First article number
    long last_article;           // Last article number
    char status;                 // Group status (y/n/m)
} ftn_newsgroup_info_t;
```

### 5. Maildir Implementation

#### Directory Structure
Create standard maildir structure:
```
/path/to/user/
├── cur/        # Read messages
├── new/        # Unread messages
├── tmp/        # Temporary files
└── .Outbox/    # Outbound messages (optional)
```

#### Message Storage Process
1. Convert FTN message to RFC822 format using existing rfc822.c
2. Generate unique filename with timestamp and process ID
3. Write to tmp/ directory first (atomic operation)
4. Move to new/ directory when complete
5. Set appropriate file permissions

#### Filename Format
Use standard maildir naming: `timestamp.pid.hostname:2,flags`

### 6. USENET Spool Implementation

#### Directory Structure
```
/var/spool/news/
├── active                    # Active newsgroups file
├── fidonet/                 # Network name
│   ├── general/             # Area name (lowercase)
│   │   ├── 1               # Article files
│   │   ├── 2
│   │   └── ...
│   └── sysop/
└── fsxnet/
    └── ...
```

#### Article Storage Process
1. Convert FTN message to RFC1036 format using existing rfc822.c
2. Determine article number (increment from active file)
3. Create directory structure if needed
4. Write article file atomically
5. Update active file with new article count

#### Active File Format
```
fidonet.general 123 1 y
fidonet.sysop 456 1 y
fsxnet.general 789 1 y
```

### 7. Path Templating Integration
- Expand %USER% to actual username
- Expand %NETWORK% to network name
- Support nested path templates
- Validate expanded paths for security

## Testing Requirements

### 1. Create Test File
Create `tests/test_storage.c` with comprehensive storage tests:

### 2. Test Cases to Implement
1. **Maildir operations**:
   - Create maildir structure
   - Store RFC822 converted messages
   - Generate unique filenames
   - Handle file system errors
   - Test atomic operations

2. **USENET spool operations**:
   - Create newsgroup directories
   - Store RFC1036 converted articles
   - Update active file correctly
   - Handle concurrent access
   - Test article numbering

3. **Path templating**:
   - Username substitution
   - Network substitution
   - Combined substitutions
   - Invalid template handling

4. **Integration testing**:
   - RFC822 conversion integration
   - RFC1036 conversion integration
   - Configuration system integration
   - Error propagation

5. **File system operations**:
   - Directory creation
   - Permission handling
   - Disk full conditions
   - Concurrent access

6. **Outbound scanning**:
   - Scan maildir outbox
   - Scan news directories
   - Convert back to FTN format
   - Handle malformed messages

### 3. Test Data
Create test files in `tests/data/`:
- Sample FTN messages for conversion testing
- Expected RFC822/RFC1036 output
- Test directory structures
- Configuration files with path templates

### 4. Mock File System
For some tests, create mock file system operations to test error conditions:
- Simulate disk full
- Simulate permission errors
- Test atomic operation failure recovery

## Integration Points

### 1. RFC822/RFC1036 Conversion
- Use existing `ftn_rfc822_from_ftn()` for maildir storage
- Use existing `ftn_rfc1036_from_ftn()` for news storage
- Use existing `ftn_rfc822_to_ftn()` for outbound scanning
- Handle conversion errors gracefully

### 2. Configuration System Integration
- Read mail and news path configurations
- Apply path templating
- Validate storage paths
- Handle configuration changes

### 3. Router Integration
- Accept routing decisions from router
- Store messages based on routing destinations
- Provide feedback on storage success/failure
- Handle storage-specific routing rules

## File Operations and Atomicity

### 1. Atomic Write Operations
- Write to temporary files first
- Use rename() for atomic completion
- Handle partial write failures
- Ensure data integrity

### 2. Locking and Concurrency
- File locking for active file updates
- Safe concurrent maildir operations
- Directory creation race condition handling
- Cross-platform locking compatibility

### 3. Error Recovery
- Cleanup temporary files on failure
- Rollback active file changes
- Retry operations with backoff
- Report detailed error information

## Performance Considerations

### 1. Efficient Operations
- Batch directory creation
- Cache active file information
- Minimize file system calls
- Optimize string operations

### 2. Scalability
- Handle large numbers of messages
- Efficient newsgroup management
- Bounded memory usage
- Fast outbound scanning

## Security Considerations

### 1. Path Validation
- Prevent directory traversal attacks
- Validate expanded path templates
- Ensure safe file permissions
- Restrict file access appropriately

### 2. Input Validation
- Sanitize usernames and area names
- Validate message content
- Handle malicious filenames
- Prevent resource exhaustion

## Success Criteria
1. All storage tests pass (aim for 20+ individual test cases)
2. Maildir storage creates proper directory structure and stores messages
3. USENET spool storage creates articles and maintains active file
4. Integration with existing RFC822/RFC1036 conversion works correctly
5. Path templating expands correctly and securely
6. Atomic operations ensure data integrity
7. Performance is acceptable for high-volume message storage
8. Error handling covers all failure scenarios
9. Memory usage is bounded and reasonable

## Files to Create/Modify
- `include/ftn/storage.h` (new)
- `src/storage.c` (new)
- `tests/test_storage.c` (new)
- `tests/data/storage_test_config.ini` (new)
- `tests/data/sample_messages/` (new directory with test messages)
- `include/ftn.h` (modify to include storage.h)
- `Makefile` (modify to include storage.c and test_storage.c)

## Future Integration
This storage system will be used by:
- Task 06: Packet processing (to store processed messages)
- Task 07: Final integration (full tosser with complete message flow)