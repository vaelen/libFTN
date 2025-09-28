# Task 2: Binkp Protocol Core Implementation

## Objective
Implement the core binkp/1.0 protocol state machine, frame parsing, and basic session management as specified in FTS-1026.

## Deliverables

### 1. Binkp Frame System
- **Files**: `src/binkp.c`, `include/ftn/binkp.h`
- **Requirements**:
  - Frame header parsing and generation (2-byte header + data)
  - Command frame vs data frame distinction (T bit)
  - Frame size validation (0-32767 bytes)
  - Frame buffering and streaming support
  - Endianness handling for network byte order

### 2. Binkp Command Implementation
- **Files**: `src/binkp_commands.c`, `include/ftn/binkp_commands.h`
- **Requirements**:
  - All standard binkp commands (M_NUL, M_ADR, M_PWD, M_OK, M_FILE, M_EOB, M_GOT, M_ERR, M_BSY, M_GET, M_SKIP)
  - Command argument parsing and validation
  - Escape sequence handling for filenames (\xHH format)
  - Address format validation (5D addressing)
  - File metadata parsing (filename, size, timestamp, offset)

### 3. Session State Machine
- **Files**: `src/binkp_session.c`, `include/ftn/binkp_session.h`
- **Requirements**:
  - Originating side state machine (Tables 1-3 from FTS-1026)
  - Answering side state machine (Tables 2-3 from FTS-1026)
  - File transfer stage implementation (Receive/Transmit routines)
  - Session setup and authentication handling
  - Proper state transitions and error handling

### 4. Basic Authentication
- **Files**: `src/binkp_auth.c`, `include/ftn/binkp_auth.h`
- **Requirements**:
  - Plain-text password authentication
  - Password validation against configuration
  - Address-based password lookup
  - Session security level tracking (secure/non-secure)

## Implementation Details

### Frame Format Implementation
```c
typedef struct {
    uint8_t header[2];  // T bit + 15-bit size
    uint8_t* data;      // Frame payload
    size_t size;        // Payload size
    int is_command;     // Command vs data frame
} ftn_binkp_frame_t;

// Frame operations
ftn_error_t ftn_binkp_frame_parse(const uint8_t* buffer, size_t len, ftn_binkp_frame_t* frame);
ftn_error_t ftn_binkp_frame_create(ftn_binkp_frame_t* frame, int is_command, const uint8_t* data, size_t size);
```

### Command System
```c
typedef enum {
    BINKP_M_NUL = 0,
    BINKP_M_ADR = 1,
    BINKP_M_PWD = 2,
    BINKP_M_FILE = 3,
    BINKP_M_OK = 4,
    BINKP_M_EOB = 5,
    BINKP_M_GOT = 6,
    BINKP_M_ERR = 7,
    BINKP_M_BSY = 8,
    BINKP_M_GET = 9,
    BINKP_M_SKIP = 10
} ftn_binkp_command_t;

typedef struct {
    ftn_binkp_command_t cmd;
    char* args;
    size_t args_len;
} ftn_binkp_command_frame_t;
```

### Session Management
```c
typedef enum {
    BINKP_STATE_INIT,
    BINKP_STATE_WAIT_ADDR,
    BINKP_STATE_WAIT_PWD,
    BINKP_STATE_WAIT_OK,
    BINKP_STATE_TRANSFER,
    BINKP_STATE_DONE,
    BINKP_STATE_ERROR
} ftn_binkp_session_state_t;

typedef struct {
    ftn_binkp_session_state_t state;
    ftn_net_connection_t* connection;
    ftn_address_t* local_addresses;
    ftn_address_t* remote_addresses;
    char* session_password;
    int is_originator;
    int is_secure;
} ftn_binkp_session_t;
```

### File Transfer Context
```c
typedef struct {
    char* filename;
    size_t file_size;
    time_t timestamp;
    size_t offset;
    FILE* file_handle;
    uint32_t crc32;  // For future CRC extension
} ftn_binkp_file_transfer_t;
```

## Protocol State Machines

### Originating Side States
1. **S0 (ConnInit)**: Establish connection
2. **S1 (WaitConn)**: Send M_NUL and M_ADR frames
3. **S2 (SendPasswd)**: Send M_PWD frame
4. **S3 (WaitAddr)**: Wait for remote M_ADR
5. **S4 (AuthRemote)**: Validate remote address
6. **S5 (IfSecure)**: Check password requirement
7. **S6 (WaitOk)**: Wait for M_OK frame
8. **S7 (Opts)**: Protocol extension negotiation
9. **T0+ (Transfer)**: File transfer stage

### Answering Side States
1. **R0 (WaitConn)**: Accept connection, send M_ADR
2. **R1 (WaitAddr)**: Wait for remote M_ADR
3. **R2 (IsPasswd)**: Check password configuration
4. **R3 (WaitPwd)**: Wait for M_PWD frame
5. **R4 (PwdAck)**: Validate password, send M_OK
6. **R5 (Opts)**: Protocol extension negotiation
7. **T0+ (Transfer)**: File transfer stage

## Error Handling Strategy

### Protocol Errors
- Invalid frame format → M_ERR + disconnect
- Unknown commands → ignore (forward compatibility)
- Authentication failure → M_ERR with explanation
- Timeout conditions → appropriate error response

### Network Errors
- Connection loss → cleanup and retry logic
- Send/receive failures → session abort
- Buffer overflow protection

## Testing Requirements

### Unit Tests
- Frame parsing with various sizes and types
- Command argument parsing and escaping
- State machine transitions for all scenarios
- Error condition handling

### Protocol Tests
- Complete session simulation (originator/answerer)
- Authentication success/failure cases
- Protocol violation handling
- Timeout and error recovery

### Integration Tests
- Real network connection establishment
- File transfer simulation
- Multi-address negotiation

## Dependencies
- Network abstraction layer from Task 1
- Configuration system for password lookup
- Logging system for protocol debugging
- File system APIs for file operations

## Acceptance Criteria
1. Can parse all binkp frame types correctly
2. Implements complete session state machines
3. Handles authentication properly
4. Manages protocol errors gracefully
5. Supports basic file transfer operations
6. Maintains protocol compliance with FTS-1026
7. Provides adequate logging for debugging
8. Handles network disconnections properly

## Protocol Compliance Notes
- Strict adherence to FTS-1026 specification
- Proper frame size validation (32767 byte limit)
- Correct endianness handling
- Character escaping in filenames
- Address format validation
- Timeout handling as specified

## Future Extension Points
- CRAM authentication hooks
- Protocol extension negotiation framework
- Compression support infrastructure
- CRC verification capability