# Task 4: Protocol Extensions Implementation

## Objective
Implement binkp protocol extensions including CRAM authentication, Non-Reliable mode, dataframe compression, and CRC checksum verification as specified in FTS-1027 through FTS-1030.

## Deliverables

### 1. CRAM Authentication (FTS-1027)
- **Files**: `src/binkp_cram.c`, `include/ftn/binkp_cram.h`
- **Requirements**:
  - Challenge-response authentication mechanism
  - MD5 and SHA1 hash function support
  - HMAC keyed hashing implementation
  - Challenge generation and validation
  - Backward compatibility with plain-text passwords

### 2. Non-Reliable Mode (FTS-1028)
- **Files**: `src/binkp_nr.c`, `include/ftn/binkp_nr.h`
- **Requirements**:
  - NR mode negotiation via M_NUL "OPT NR"
  - Offset-based file resume capability
  - M_GET frame with explicit offset handling
  - File position tracking and management
  - Performance optimization for unreliable connections

### 3. Dataframe Compression (FTS-1029)
- **Files**: `src/binkp_plz.c`, `include/ftn/binkp_plz.h`
- **Requirements**:
  - PLZ mode negotiation and activation
  - ZLib compression/decompression
  - Compressed frame indicator (second high bit)
  - Dynamic compression control (NZ parameter)
  - Frame size limit handling (16383 bytes in PLZ mode)

### 4. CRC Checksum Verification (FTS-1030)
- **Files**: `src/binkp_crc.c`, `include/ftn/binkp_crc.h`
- **Requirements**:
  - CRC32 checksum calculation
  - Extended M_FILE command with checksum parameter
  - File integrity verification
  - Error handling and recovery (M_SKIP/M_ERR responses)
  - ITU-T V.42 CRC-32 algorithm implementation

## Implementation Details

### CRAM Authentication System
```c
typedef struct {
    char* hash_algorithms[8];  // Supported hash functions
    uint8_t challenge_data[64]; // Challenge bytes
    size_t challenge_len;
    char* selected_algorithm;
} ftn_cram_context_t;

// CRAM operations
ftn_error_t ftn_cram_generate_challenge(ftn_cram_context_t* ctx);
ftn_error_t ftn_cram_create_response(const char* password, const uint8_t* challenge,
                                   size_t challenge_len, const char* algorithm,
                                   char** response);
ftn_error_t ftn_cram_verify_response(const char* password, const uint8_t* challenge,
                                   size_t challenge_len, const char* algorithm,
                                   const char* response);
```

### HMAC Implementation
```c
// HMAC-MD5 and HMAC-SHA1 implementation
ftn_error_t ftn_hmac_md5(const uint8_t* key, size_t key_len,
                        const uint8_t* data, size_t data_len,
                        uint8_t* digest);
ftn_error_t ftn_hmac_sha1(const uint8_t* key, size_t key_len,
                         const uint8_t* data, size_t data_len,
                         uint8_t* digest);

// Hex encoding/decoding utilities
char* ftn_bytes_to_hex(const uint8_t* bytes, size_t len, int lowercase);
ftn_error_t ftn_hex_to_bytes(const char* hex, uint8_t** bytes, size_t* len);
```

### Non-Reliable Mode Support
```c
typedef struct {
    int nr_mode_active;
    int nr_mode_supported;
    size_t resume_offset;
    char* resume_filename;
    time_t resume_timestamp;
} ftn_nr_context_t;

// NR mode operations
ftn_error_t ftn_nr_request_offset(ftn_binkp_session_t* session,
                                const char* filename, size_t size,
                                time_t timestamp, size_t offset);
ftn_error_t ftn_nr_handle_get_request(ftn_transfer_context_t* transfer,
                                    const char* filename, size_t size,
                                    time_t timestamp, size_t offset);
```

### Compression Support
```c
typedef struct {
    int plz_mode_active;
    int plz_mode_supported;
    z_stream compress_stream;
    z_stream decompress_stream;
    uint8_t* compress_buffer;
    size_t compress_buffer_size;
} ftn_plz_context_t;

// Compression operations
ftn_error_t ftn_plz_compress_frame(ftn_plz_context_t* ctx,
                                 const uint8_t* input, size_t input_len,
                                 uint8_t** output, size_t* output_len);
ftn_error_t ftn_plz_decompress_frame(ftn_plz_context_t* ctx,
                                   const uint8_t* input, size_t input_len,
                                   uint8_t** output, size_t* output_len);
```

### CRC Checksum System
```c
typedef struct {
    int crc_mode_active;
    int crc_mode_supported;
    uint32_t file_crc;
    uint32_t calculated_crc;
} ftn_crc_context_t;

// CRC operations
uint32_t ftn_crc32_calculate(const uint8_t* data, size_t len);
uint32_t ftn_crc32_file(FILE* file);
ftn_error_t ftn_crc32_verify_file(const char* filename, uint32_t expected_crc);

// CRC-32 lookup table
extern const uint32_t ftn_crc32_table[256];
```

## Protocol Extension Integration

### Extension Negotiation
```c
typedef struct {
    ftn_cram_context_t* cram;
    ftn_nr_context_t* nr;
    ftn_plz_context_t* plz;
    ftn_crc_context_t* crc;
    char* extension_string;  // "OPT CRAM-MD5-abc123 NR PLZ CRC"
} ftn_protocol_extensions_t;

// Extension negotiation
ftn_error_t ftn_extensions_parse_opt(const char* opt_string,
                                   ftn_protocol_extensions_t* ext);
ftn_error_t ftn_extensions_generate_opt(ftn_protocol_extensions_t* ext,
                                      char** opt_string);
```

### Enhanced Session Context
```c
// Extended session structure with protocol extensions
typedef struct {
    // ... existing session fields ...
    ftn_protocol_extensions_t extensions;

    // Extension state flags
    int cram_required;
    int nr_preferred;
    int plz_enabled;
    int crc_enabled;
} ftn_binkp_session_extended_t;
```

## CRAM Authentication Flow

### Challenge Generation (Answering Side)
1. Generate random challenge data (8-64 bytes)
2. Create M_NUL "OPT CRAM-MD5/SHA1-hexdata" frame
3. Send as first frame before M_ADR
4. Store challenge for later verification

### Response Generation (Originating Side)
1. Parse challenge from M_NUL "OPT CRAM-..." frame
2. Select supported hash algorithm
3. Calculate HMAC digest using password as key
4. Send M_PWD "CRAM-algorithm-hexdigest" frame
5. Fallback to plain-text if CRAM not supported

### Verification Process
```c
ftn_error_t verify_cram_response(const char* password,
                               const uint8_t* challenge, size_t challenge_len,
                               const char* algorithm, const char* response) {
    uint8_t expected_digest[20];  // Max for SHA1
    size_t digest_len;

    if (strcmp(algorithm, "MD5") == 0) {
        ftn_hmac_md5((uint8_t*)password, strlen(password),
                    challenge, challenge_len, expected_digest);
        digest_len = 16;
    } else if (strcmp(algorithm, "SHA1") == 0) {
        ftn_hmac_sha1((uint8_t*)password, strlen(password),
                     challenge, challenge_len, expected_digest);
        digest_len = 20;
    } else {
        return FTN_ERROR_UNSUPPORTED;
    }

    char* expected_hex = ftn_bytes_to_hex(expected_digest, digest_len, 1);
    int result = (strcmp(response, expected_hex) == 0) ?
                 FTN_ERROR_OK : FTN_ERROR_AUTH_FAILED;
    free(expected_hex);

    return result;
}
```

## Non-Reliable Mode Implementation

### File Resume Logic
```c
ftn_error_t handle_nr_file_request(ftn_transfer_context_t* transfer,
                                 const char* filename, size_t size,
                                 time_t timestamp, size_t offset) {
    // Find matching file in transfer queue
    ftn_file_transfer_t* file = find_transfer_file(transfer, filename, size, timestamp);
    if (!file) {
        return FTN_ERROR_FILE_NOT_FOUND;
    }

    // Validate offset
    if (offset > size) {
        return FTN_ERROR_INVALID_OFFSET;
    }

    // Seek to requested position
    if (fseek(file->file_handle, offset, SEEK_SET) != 0) {
        return FTN_ERROR_FILE_IO;
    }

    file->transferred = offset;
    return FTN_ERROR_OK;
}
```

## Compression Implementation

### Frame Compression
```c
ftn_error_t compress_data_frame(ftn_plz_context_t* ctx,
                              const uint8_t* input, size_t input_len,
                              ftn_binkp_frame_t* frame) {
    if (!ctx->plz_mode_active || input_len < 64) {
        // Don't compress small frames or when PLZ disabled
        return create_uncompressed_frame(input, input_len, frame);
    }

    uint8_t* compressed;
    size_t compressed_len;

    int result = ftn_plz_compress_frame(ctx, input, input_len,
                                      &compressed, &compressed_len);
    if (result != FTN_ERROR_OK || compressed_len >= input_len) {
        // Compression failed or didn't help
        return create_uncompressed_frame(input, input_len, frame);
    }

    // Create compressed frame with special header bit
    frame->header[0] = 0x80 | ((compressed_len >> 8) & 0x3F);  // Set compression bit
    frame->header[1] = compressed_len & 0xFF;
    frame->data = compressed;
    frame->size = compressed_len;
    frame->is_command = 0;

    return FTN_ERROR_OK;
}
```

## CRC Implementation

### CRC-32 Algorithm (ITU-T V.42)
```c
// Pre-computed CRC-32 lookup table
const uint32_t ftn_crc32_table[256] = {
    0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
    // ... full 256-entry table ...
};

uint32_t ftn_crc32_calculate(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;  // Preconditioning

    for (size_t i = 0; i < len; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ ftn_crc32_table[index];
    }

    return ~crc;  // Postconditioning
}
```

### Extended M_FILE Processing
```c
ftn_error_t parse_extended_mfile(const char* args, ftn_file_info_t* info) {
    // Parse: "filename size timestamp offset [crc32]"
    char* tokens[5];
    int token_count = tokenize_string(args, tokens, 5);

    if (token_count < 4) {
        return FTN_ERROR_PARSE;
    }

    info->filename = strdup(tokens[0]);
    info->size = strtoul(tokens[1], NULL, 10);
    info->timestamp = strtoul(tokens[2], NULL, 10);
    info->offset = strtoul(tokens[3], NULL, 10);

    if (token_count == 5) {
        info->crc32 = strtoul(tokens[4], NULL, 16);
        info->has_crc = 1;
    } else {
        info->has_crc = 0;
    }

    return FTN_ERROR_OK;
}
```

## Testing Requirements

### CRAM Authentication Tests
- Challenge generation and parsing
- HMAC calculation with test vectors
- Response verification
- Fallback to plain-text authentication

### Non-Reliable Mode Tests
- File resume from various offsets
- M_GET request handling
- Large file transfer interruption/resume

### Compression Tests
- Frame compression/decompression
- Compression effectiveness measurement
- Dynamic compression control

### CRC Verification Tests
- CRC calculation with known test data
- File integrity verification
- Error detection and recovery

## Dependencies
- Cryptographic libraries (MD5, SHA1)
- ZLib compression library
- Enhanced binkp protocol from Task 2
- File transfer system from Task 3

## Acceptance Criteria
1. CRAM authentication works with MD5 and SHA1
2. Non-reliable mode enables file resume
3. Compression reduces data transfer when beneficial
4. CRC verification detects file corruption
5. All extensions are backward compatible
6. Extension negotiation works properly
7. Performance is improved for target use cases
8. Error handling is robust and informative

## Security Considerations
- Secure random number generation for challenges
- Proper key derivation for HMAC
- Protection against timing attacks
- Secure memory handling for passwords
- Validation of all input parameters

## Performance Optimization
- Efficient compression threshold selection
- Minimal overhead for CRC calculation
- Optimized HMAC implementations
- Smart buffering for compressed transfers