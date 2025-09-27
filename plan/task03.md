# Task 03: Duplicate Detection System

## Objective
Implement a robust duplicate detection system based on MSGID control paragraphs to prevent message duplication as packets propagate through the FTN network.

## Requirements
1. Extract and normalize MSGID values from FTN messages
2. Persistent storage of seen message IDs with efficient lookup
3. Age-based cleanup of old duplicate records
4. Thread-safe operations for potential future multi-threading
5. Configurable retention policies
6. Integration with existing FTN message parsing

## Implementation Steps

### 1. Create Header File
Create `include/ftn/dupecheck.h` with:
- Duplicate checker structure and function declarations
- Configuration options for retention policies
- Error codes for duplicate detection operations

### 2. Create Implementation File
Create `src/dupecheck.c` with:
- MSGID extraction and normalization functions
- Simple database implementation for seen message storage
- Cleanup and maintenance functions

### 3. Core Functions to Implement
```c
// Duplicate checker lifecycle
ftn_dupecheck_t* ftn_dupecheck_new(const char* db_path);
void ftn_dupecheck_free(ftn_dupecheck_t* dupecheck);
ftn_error_t ftn_dupecheck_load(ftn_dupecheck_t* dupecheck);
ftn_error_t ftn_dupecheck_save(ftn_dupecheck_t* dupecheck);

// Duplicate detection operations
ftn_error_t ftn_dupecheck_is_duplicate(ftn_dupecheck_t* dupecheck, const ftn_message_t* msg, int* is_dupe);
ftn_error_t ftn_dupecheck_add_message(ftn_dupecheck_t* dupecheck, const ftn_message_t* msg);

// MSGID operations
char* ftn_dupecheck_extract_msgid(const ftn_message_t* msg);
char* ftn_dupecheck_normalize_msgid(const char* msgid);

// Maintenance operations
ftn_error_t ftn_dupecheck_cleanup_old(ftn_dupecheck_t* dupecheck, time_t cutoff_time);
ftn_error_t ftn_dupecheck_get_stats(const ftn_dupecheck_t* dupecheck, ftn_dupecheck_stats_t* stats);
```

### 4. Data Structures
```c
typedef struct {
    char* db_path;                    // Path to duplicate database file
    void* db_handle;                  // Internal database handle
    time_t retention_days;            // How long to keep records
    size_t max_entries;               // Maximum number of entries
} ftn_dupecheck_t;

typedef struct {
    size_t total_entries;             // Total messages in database
    size_t entries_cleaned;           // Entries removed in last cleanup
    time_t last_cleanup;              // Last cleanup timestamp
    time_t oldest_entry;              // Oldest message timestamp
} ftn_dupecheck_stats_t;
```

### 5. MSGID Processing
- Extract MSGID from FTN message control paragraphs
- Handle various MSGID formats and encodings
- Normalize MSGIDs for consistent comparison
- Handle missing or malformed MSGIDs gracefully

### 6. Database Implementation
Use a simple text-based database format for maximum portability:
- One line per MSGID with timestamp
- Format: `timestamp|normalized_msgid`
- Use file locking for concurrent access safety
- Efficient loading and searching algorithms

## Testing Requirements

### 1. Create Test File
Create `tests/test_dupecheck.c` with comprehensive tests:

### 2. Test Cases to Implement
1. **MSGID extraction**:
   - Extract MSGID from FTN messages with various formats
   - Handle messages without MSGID
   - Handle malformed MSGID control paragraphs
   - Test MSGID normalization

2. **Duplicate detection**:
   - Detect actual duplicates
   - Handle first-time messages
   - Handle near-duplicates with different timestamps
   - Performance with large numbers of messages

3. **Database operations**:
   - Create new database
   - Load existing database
   - Save database to disk
   - Handle file access errors

4. **Cleanup operations**:
   - Age-based cleanup
   - Size-based cleanup
   - Cleanup statistics
   - Empty database handling

5. **Error conditions**:
   - Invalid database path
   - Corrupted database file
   - Disk full conditions
   - Memory allocation failures

### 3. Test Data
Create test files in `tests/data/`:
- `test_dupececk.db` - Sample duplicate database
- Sample FTN messages with various MSGID formats
- Messages with and without MSGID control paragraphs

### 4. Performance Tests
- Test with large numbers of messages (10,000+)
- Measure lookup performance
- Test memory usage patterns
- Verify cleanup efficiency

## Integration Points

### 1. FTN Message Integration
- Use existing `ftn_message_t` structure from libftn
- Leverage existing control paragraph parsing in packet.c
- Integrate with message routing decisions

### 2. Configuration Integration
- Add duplicate checking settings to configuration system
- Database path configuration
- Retention policy settings
- Cleanup frequency configuration

### 3. Logging Integration
- Log duplicate detection events
- Log cleanup operations
- Log database errors
- Statistics reporting

## Database Format Specification

### 1. File Format
```
# libFTN Duplicate Database v1.0
# timestamp|msgid
1234567890|1:2/3@fidonet 12345678
1234567891|21:1/100@fsxnet abcdef01
```

### 2. File Operations
- Atomic writes using temporary files
- File locking for concurrent access
- Backup and recovery mechanisms
- Compression for large databases

## Performance Considerations

### 1. Lookup Optimization
- In-memory hash table for frequently accessed entries
- Binary search for disk-based lookups
- LRU cache for recent lookups
- Batch operations for multiple messages

### 2. Memory Management
- Limit in-memory database size
- Efficient string handling
- Proper cleanup of temporary data
- Memory usage monitoring

## Success Criteria
1. All duplicate detection tests pass (aim for 20+ individual test cases)
2. MSGID extraction works with various FTN message formats
3. Database operations are reliable and atomic
4. Performance is acceptable with 10,000+ messages
5. Memory usage is bounded and reasonable
6. Integration with existing libftn message structures is seamless
7. Error handling covers all failure modes

## Files to Create/Modify
- `include/ftn/dupecheck.h` (new)
- `src/dupecheck.c` (new)
- `tests/test_dupecheck.c` (new)
- `tests/data/test_dupecheck.db` (new)
- `include/ftn.h` (modify to include dupecheck.h)
- `Makefile` (modify to include dupecheck.c and test_dupecheck.c)

## Future Integration
This duplicate detection system will be used by:
- Task 04: Message routing engine (to skip duplicate messages)
- Task 06: Packet processing (to filter duplicates during processing)
- Task 07: Final integration (configuration and logging integration)