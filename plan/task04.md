# Task 04: Message Routing Engine

## Objective
Implement a message routing engine that analyzes FTN message headers and addressing to determine appropriate routing decisions (local delivery, forwarding, echomail distribution).

## Requirements
1. Analyze FTN message addressing and determine routing destinations
2. Support multi-network configurations with different domains
3. Route netmail to local users or forward to other nodes
4. Route echomail to appropriate newsgroup areas
5. Handle address validation and network resolution
6. Integration with duplicate detection system
7. Support for routing rules and policies

## Implementation Steps

### 1. Create Header File
Create `include/ftn/router.h` with:
- Router structure and configuration
- Routing decision enumeration
- Function declarations for routing operations

### 2. Create Implementation File
Create `src/router.c` with:
- Message analysis and routing decision logic
- Address resolution and validation
- Network-specific routing rules
- Integration with existing libftn address functions

### 3. Core Functions to Implement
```c
// Router lifecycle
ftn_router_t* ftn_router_new(const ftn_config_t* config);
void ftn_router_free(ftn_router_t* router);

// Routing operations
ftn_error_t ftn_router_route_message(ftn_router_t* router, const ftn_message_t* msg, ftn_routing_decision_t* decision);
ftn_error_t ftn_router_validate_address(ftn_router_t* router, const ftn_address_t* addr, const char* network);

// Address resolution
ftn_error_t ftn_router_resolve_destination(ftn_router_t* router, const ftn_message_t* msg, ftn_destination_t* dest);
ftn_error_t ftn_router_is_local_address(ftn_router_t* router, const ftn_address_t* addr, const char* network, int* is_local);

// Routing rules
ftn_error_t ftn_router_add_rule(ftn_router_t* router, const ftn_routing_rule_t* rule);
ftn_error_t ftn_router_remove_rule(ftn_router_t* router, const char* rule_name);
```

### 4. Data Structures
```c
typedef enum {
    FTN_ROUTE_LOCAL_MAIL,      // Deliver to local user mailbox
    FTN_ROUTE_LOCAL_NEWS,      // Post to local newsgroup
    FTN_ROUTE_FORWARD,         // Forward to another node
    FTN_ROUTE_BOUNCE,          // Return to sender (undeliverable)
    FTN_ROUTE_DROP             // Discard message
} ftn_routing_action_t;

typedef struct {
    ftn_routing_action_t action;
    char* destination_path;     // Path for local delivery
    char* destination_user;     // Username for mail delivery
    char* destination_area;     // Area name for news delivery
    ftn_address_t forward_to;   // Address for forwarding
    char* network_name;         // Network for routing
} ftn_routing_decision_t;

typedef struct {
    const ftn_config_t* config;
    ftn_dupecheck_t* dupecheck;
    void* routing_rules;        // Internal routing rules storage
} ftn_router_t;

typedef struct {
    char* name;                 // Rule name
    char* pattern;              // Address/area pattern
    ftn_routing_action_t action;
    char* parameter;            // Action-specific parameter
} ftn_routing_rule_t;
```

### 5. Routing Logic Implementation

#### Message Type Analysis
- Identify netmail vs echomail based on message attributes
- Extract destination addresses and areas
- Validate message structure and required fields

#### Address Resolution
- Parse FTN addresses using existing libftn functions
- Resolve addresses within configured networks
- Determine if addresses are local to this node
- Handle point addressing and routing

#### Routing Decision Matrix
```
Message Type | Destination      | Action
-------------|------------------|------------------
Netmail      | Local address    | LOCAL_MAIL
Netmail      | Remote address   | FORWARD
Echomail     | Subscribed area  | LOCAL_NEWS
Echomail     | Unknown area     | DROP
Echomail     | Transit only     | FORWARD
```

### 6. Network Integration
- Use configuration system to determine local networks
- Support multiple simultaneous networks
- Handle inter-network routing and gateways
- Address validation per network standards

## Testing Requirements

### 1. Create Test File
Create `tests/test_router.c` with comprehensive routing tests:

### 2. Test Cases to Implement
1. **Message analysis**:
   - Distinguish netmail from echomail
   - Extract addressing information
   - Handle malformed messages
   - Validate required fields

2. **Address resolution**:
   - Local address detection
   - Remote address routing
   - Point address handling
   - Invalid address handling

3. **Routing decisions**:
   - Local mail delivery routing
   - Local news delivery routing
   - Message forwarding routing
   - Message bouncing/dropping

4. **Multi-network support**:
   - Route within single network
   - Route between networks
   - Network-specific address validation
   - Default network handling

5. **Routing rules**:
   - Add and remove custom rules
   - Pattern matching for addresses/areas
   - Rule priority and ordering
   - Rule validation

6. **Integration testing**:
   - Configuration system integration
   - Duplicate detection integration
   - Error handling and recovery
   - Performance with large message volumes

### 3. Test Data
Create test files in `tests/data/`:
- Sample FTN messages with various addressing
- Multi-network configuration files
- Test routing rule configurations
- Edge case message formats

### 4. Mock Dependencies
Create mock implementations for testing:
- Mock configuration system
- Mock duplicate detection
- Mock message structures

## Integration Points

### 1. Configuration System Integration
- Load network configurations for routing decisions
- Use node addresses for local detection
- Apply network-specific routing rules
- Handle configuration changes

### 2. Duplicate Detection Integration
- Check for duplicates before routing
- Skip routing for duplicate messages
- Update duplicate database after routing decisions
- Handle duplicate detection errors

### 3. Existing libFTN Integration
- Use existing FTN address parsing functions
- Leverage existing message structure definitions
- Integrate with nodelist lookup functionality
- Use existing error handling patterns

## Routing Rules System

### 1. Rule Definition Format
```ini
# Routing rules can be defined in configuration
[routing_rules]
rule1 = area:FIDONET.* -> LOCAL_NEWS:/var/spool/news/fidonet
rule2 = addr:1:*/* -> FORWARD:1:1/100
rule3 = area:TEST.* -> DROP
```

### 2. Pattern Matching
- Support wildcards in area names and addresses
- Regular expression support for complex patterns
- Priority-based rule evaluation
- Default rule handling

### 3. Rule Actions
- LOCAL_MAIL: Deliver to user mailbox
- LOCAL_NEWS: Post to newsgroup
- FORWARD: Send to specified node
- BOUNCE: Return to sender
- DROP: Discard silently

## Performance Considerations

### 1. Routing Performance
- Cache frequently used routing decisions
- Optimize address comparison operations
- Minimize string operations and allocations
- Batch processing for multiple messages

### 2. Memory Management
- Efficient storage of routing rules
- Proper cleanup of routing decisions
- Bounded memory usage for caches
- Handle memory allocation failures

## Error Handling

### 1. Routing Errors
- Invalid destination addresses
- Unknown networks or areas
- Configuration errors
- Resource exhaustion

### 2. Recovery Strategies
- Fallback routing rules
- Dead letter handling
- Error reporting and logging
- Graceful degradation

## Success Criteria
1. All routing tests pass (aim for 25+ individual test cases)
2. Correctly routes messages based on FTN addressing standards
3. Multi-network configuration support works properly
4. Integration with configuration and duplicate detection systems
5. Performance is acceptable for high-volume message processing
6. Error handling covers all failure scenarios
7. Memory usage is bounded and reasonable
8. Code follows existing libftn conventions

## Files to Create/Modify
- `include/ftn/router.h` (new)
- `src/router.c` (new)
- `tests/test_router.c` (new)
- `tests/data/router_test_config.ini` (new)
- `tests/data/router_test_messages.dat` (new)
- `include/ftn.h` (modify to include router.h)
- `Makefile` (modify to include router.c and test_router.c)

## Future Integration
This routing engine will be used by:
- Task 05: Storage implementation (to determine storage destinations)
- Task 06: Packet processing (to route messages during processing)
- Task 07: Final integration (full routing implementation with all components)