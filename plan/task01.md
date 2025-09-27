# Task 01: Configuration System Implementation

## Objective
Create a robust INI configuration parser for the FTN tosser with support for case-insensitive sections/keys and path templating.

## Requirements
1. Parse INI files with sections: [node], [news], [mail], [network_name]
2. Support case-insensitive section names and keys
3. Implement path templating for %USER% and %NETWORK% substitutions
4. Validate configuration completeness and report errors
5. Support multiple network sections in a single configuration file

## Implementation Steps

### 1. Create Header File
Create `include/ftn/config.h` with:
- Configuration structures for each section type
- Function declarations for parsing and validation
- Error codes specific to configuration parsing

### 2. Create Implementation File
Create `src/config.c` with:
- INI parser that handles comments, whitespace, and case-insensitivity
- Configuration structure initialization and cleanup
- Path templating engine
- Configuration validation functions

### 3. Core Functions to Implement
```c
// Main configuration functions
ftn_config_t* ftn_config_new(void);
void ftn_config_free(ftn_config_t* config);
ftn_error_t ftn_config_load(ftn_config_t* config, const char* filename);
ftn_error_t ftn_config_validate(const ftn_config_t* config);

// Path templating functions
char* ftn_config_expand_path(const char* template, const char* user, const char* network);

// Accessor functions for configuration sections
const ftn_node_config_t* ftn_config_get_node(const ftn_config_t* config);
const ftn_mail_config_t* ftn_config_get_mail(const ftn_config_t* config);
const ftn_news_config_t* ftn_config_get_news(const ftn_config_t* config);
const ftn_network_config_t* ftn_config_get_network(const ftn_config_t* config, const char* name);
```

### 4. Configuration Structures
Define structures that map to the INI sections:
- `ftn_node_config_t` - Node information and network list
- `ftn_mail_config_t` - Mail directory configurations
- `ftn_news_config_t` - News spool configuration
- `ftn_network_config_t` - Network-specific settings
- `ftn_config_t` - Master configuration container

### 5. Error Handling
Add appropriate error codes to the main `ftn_error_t` enum:
- `FTN_ERROR_CONFIG_PARSE`
- `FTN_ERROR_CONFIG_MISSING_SECTION`
- `FTN_ERROR_CONFIG_MISSING_KEY`
- `FTN_ERROR_CONFIG_INVALID_VALUE`

## Testing Requirements

### 1. Create Test File
Create `tests/test_config.c` with comprehensive tests:

### 2. Test Cases to Implement
1. **Basic INI parsing**:
   - Parse valid configuration file
   - Handle comments and whitespace
   - Case-insensitive section and key names

2. **Path templating**:
   - %USER% substitution
   - %NETWORK% substitution
   - Combined substitutions
   - No substitution needed

3. **Configuration validation**:
   - Missing required sections
   - Missing required keys
   - Invalid values
   - Complete valid configuration

4. **Multi-network support**:
   - Multiple network sections
   - Network lookup by name
   - Default network handling

5. **Error handling**:
   - Malformed INI syntax
   - File not found
   - Permission errors
   - Memory allocation failures

### 3. Test Data
Create sample configuration files in `tests/data/`:
- `valid_config.ini` - Complete valid configuration
- `invalid_syntax.ini` - Malformed INI syntax
- `missing_sections.ini` - Missing required sections
- `multi_network.ini` - Multiple network configuration

### 4. Memory Management Tests
- Verify proper cleanup with valgrind or similar
- Test memory allocation failure paths
- Ensure no memory leaks in error conditions

## Success Criteria
1. All configuration tests pass (aim for 15+ individual test cases)
2. Configuration parser handles the example configuration from FTNTOSS.md
3. Path templating works correctly for all substitution patterns
4. Error reporting provides clear, actionable error messages
5. Memory management is leak-free
6. Code follows existing libftn conventions and style

## Integration Notes
- Use existing libftn error handling patterns
- Follow ANSI C (C89) compliance requirements
- Integrate with existing Makefile build system
- Ensure compatibility with existing compat.c functionality

## Files to Create/Modify
- `include/ftn/config.h` (new)
- `src/config.c` (new)
- `tests/test_config.c` (new)
- `tests/data/valid_config.ini` (new)
- `tests/data/invalid_syntax.ini` (new)
- `tests/data/missing_sections.ini` (new)
- `tests/data/multi_network.ini` (new)
- `include/ftn.h` (modify to include config.h)
- `Makefile` (modify to include config.c and test_config.c)