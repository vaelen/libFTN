# Task 06: Packet Processing Implementation

## Objective
Implement the core packet processing workflow that integrates all previous components (configuration, duplicate detection, routing, storage) to process FTN packets and handle both inbound and outbound message flows.

## Requirements
1. Scan configured inbox directories for FTN packets
2. Process packets using existing libftn packet parsing functionality
3. Integrate duplicate detection, routing, and storage systems
4. Handle malformed packets (move to "bad" folder)
5. Move processed packets to "processed" folder
6. Create outbound packets from stored messages
7. Support for multiple network configurations
8. Comprehensive error handling and logging

## Implementation Steps

### 1. Update Main Tosser Implementation
Enhance `src/ftntoss.c` with complete packet processing logic:
- Replace placeholder functions with full implementation
- Integrate all component systems
- Add comprehensive error handling and logging

### 2. Core Processing Functions to Implement
```c
// Main processing workflows
int process_inbox(const ftn_config_t* config);
int process_network_inbox(const ftn_network_config_t* network, ftn_router_t* router, ftn_storage_t* storage, ftn_dupecheck_t* dupecheck);
int process_single_packet(const char* packet_path, const ftn_network_config_t* network, ftn_router_t* router, ftn_storage_t* storage, ftn_dupecheck_t* dupecheck);

// Outbound processing
int process_outbound(const ftn_config_t* config);
int create_outbound_packets(const ftn_network_config_t* network, ftn_storage_t* storage);

// Packet management
int move_packet_to_processed(const char* packet_path, const char* processed_dir);
int move_packet_to_bad(const char* packet_path, const char* bad_dir);
int ensure_directories_exist(const ftn_network_config_t* network);
```

### 3. Integration Workflow
```
For each network:
  1. Ensure directories exist (inbox, outbox, processed, bad)
  2. Scan inbox for FTN packet files
  3. For each packet:
     a. Parse packet using libftn packet.c
     b. For each message in packet:
        - Check for duplicates
        - Determine routing destination
        - Store message if not duplicate
     c. Move packet to processed/ or bad/ directory
  4. Scan outbound areas for messages
  5. Create outbound packets for messages
  6. Place packets in outbox directory
```

### 4. Message Processing Flow
```c
// Per-message processing logic
ftn_error_t process_message(const ftn_message_t* msg, const ftn_network_config_t* network,
                           ftn_router_t* router, ftn_storage_t* storage, ftn_dupecheck_t* dupecheck) {

    // 1. Check for duplicates
    int is_duplicate;
    if (ftn_dupecheck_is_duplicate(dupecheck, msg, &is_duplicate) != FTN_OK) {
        return FTN_ERROR_DUPECHECK;
    }

    if (is_duplicate) {
        log_info("Skipping duplicate message: %s", msg->msgid);
        return FTN_OK;
    }

    // 2. Determine routing
    ftn_routing_decision_t decision;
    if (ftn_router_route_message(router, msg, &decision) != FTN_OK) {
        return FTN_ERROR_ROUTING;
    }

    // 3. Store message based on routing decision
    switch (decision.action) {
        case FTN_ROUTE_LOCAL_MAIL:
            return ftn_storage_store_mail(storage, msg, decision.destination_user, network->name);
        case FTN_ROUTE_LOCAL_NEWS:
            return ftn_storage_store_news(storage, msg, decision.destination_area, network->name);
        case FTN_ROUTE_FORWARD:
            // Add to outbound queue (to be implemented)
            return queue_for_forwarding(msg, &decision.forward_to, network);
        case FTN_ROUTE_DROP:
            log_info("Dropping message per routing rules: %s", msg->msgid);
            return FTN_OK;
        default:
            return FTN_ERROR_INVALID;
    }
}
```

### 5. Directory Management
- Create required directories if they don't exist
- Handle permission errors gracefully
- Validate directory accessibility
- Support configurable directory structure

### 6. File Processing
- Scan directories for FTN packet files (*.pkt)
- Use existing libftn packet parsing functions
- Handle compressed packets (optional future enhancement)
- Atomic file operations for moves

### 7. Error Handling and Recovery
- Malformed packet handling (move to bad/ directory)
- Partial processing recovery
- Logging of all processing events
- Statistics collection and reporting

## Testing Requirements

### 1. Create Integration Test File
Create `tests/test_integration.c` for end-to-end testing:

### 2. Test Cases to Implement
1. **Complete packet processing workflow**:
   - Process valid packets with netmail and echomail
   - Handle mixed message types in single packet
   - Verify correct storage destinations
   - Check duplicate detection integration

2. **Error handling**:
   - Malformed packet handling
   - Missing directories
   - Permission errors
   - Disk full conditions

3. **Multi-network processing**:
   - Process packets from different networks
   - Network-specific routing
   - Cross-network message handling
   - Configuration validation

4. **Outbound processing**:
   - Create packets from stored messages
   - Proper addressing and routing
   - Outbound queue management
   - Packet creation validation

5. **File management**:
   - Packet file discovery
   - Atomic file operations
   - Directory creation
   - File move operations

6. **Performance testing**:
   - Large packet processing
   - Multiple simultaneous packets
   - Memory usage validation
   - Processing time measurements

### 3. Test Data Creation
Create comprehensive test data in `tests/data/`:
- `test_packets/` - Directory with various FTN packet files
- `test_config_integration.ini` - Configuration for integration testing
- `expected_results/` - Expected output for validation
- `malformed_packets/` - Invalid packets for error testing

### 4. Test Environment Setup
- Create temporary directory structure for testing
- Mock file system operations where needed
- Validate cleanup after test completion
- Support for concurrent test execution

## Enhanced Error Handling

### 1. Error Classification
```c
typedef enum {
    FTN_PROCESS_SUCCESS,
    FTN_PROCESS_ERROR_PACKET_PARSE,
    FTN_PROCESS_ERROR_DUPLICATE,
    FTN_PROCESS_ERROR_ROUTING,
    FTN_PROCESS_ERROR_STORAGE,
    FTN_PROCESS_ERROR_FILE_MOVE,
    FTN_PROCESS_ERROR_DIRECTORY,
    FTN_PROCESS_ERROR_PERMISSION
} ftn_process_error_t;
```

### 2. Logging Integration
- Detailed logging of processing steps
- Error context preservation
- Processing statistics
- Debugging information

### 3. Recovery Strategies
- Retry mechanisms for transient errors
- Graceful degradation for partial failures
- Dead letter handling for unprocessable messages
- Administrative notifications for critical errors

## Performance Optimizations

### 1. Batch Processing
- Process multiple packets efficiently
- Minimize file system operations
- Optimize memory usage patterns
- Cache frequently accessed data

### 2. Resource Management
- Limit memory usage for large packets
- Control disk I/O patterns
- Efficient string handling
- Proper resource cleanup

## Statistics and Monitoring

### 1. Processing Statistics
```c
typedef struct {
    size_t packets_processed;
    size_t messages_processed;
    size_t duplicates_found;
    size_t messages_stored;
    size_t messages_forwarded;
    size_t errors_encountered;
    time_t processing_start_time;
    time_t processing_end_time;
} ftn_processing_stats_t;
```

### 2. Monitoring Functions
- Real-time processing statistics
- Performance metrics collection
- Error rate monitoring
- Resource usage tracking

## Success Criteria
1. All integration tests pass (aim for 15+ comprehensive test scenarios)
2. Complete packet processing workflow functions correctly
3. All component systems integrate seamlessly
4. Error handling covers all failure modes
5. Performance is acceptable for production use
6. Memory usage is bounded and reasonable
7. File operations are atomic and reliable
8. Statistics and logging provide adequate monitoring
9. Code follows existing libftn conventions

## Files to Create/Modify
- `src/ftntoss.c` (enhance existing implementation)
- `tests/test_integration.c` (new)
- `tests/data/test_packets/` (new directory with test packets)
- `tests/data/test_config_integration.ini` (new)
- `tests/data/expected_results/` (new directory)
- `tests/data/malformed_packets/` (new directory)
- `Makefile` (modify to include integration tests)

## Integration Dependencies
This task requires successful completion of:
- Task 01: Configuration system
- Task 02: Main tosser structure
- Task 03: Duplicate detection system
- Task 04: Message routing engine
- Task 05: Storage implementation

## Future Enhancement Areas
- Compressed packet support
- Advanced routing rules
- Performance optimizations
- Administrative interfaces
- Network statistics and monitoring