# Plan: Consolidate Storage Code

## Objective
Consolidate the duplicate code between the storage implementation and the existing pkt2mail and pkt2news utilities by:
1. Moving core functionality from utilities into the storage library
2. Updating storage library to match utility behavior exactly
3. Refactoring utilities to use storage library functions
4. Ensuring all existing behavior is preserved

## Current Code Analysis

### pkt2mail.c Functionality
- Path templating with %USER% substitution (lines 67-113)
- Maildir creation (lines 116-159) - **DUPLICATES storage.c**
- Filename generation using MSGID or timestamp (lines 162-221)
- Message duplicate detection (lines 224-279)
- RFC822 conversion and atomic maildir storage (lines 282-370)
- Command-line argument parsing
- Main processing loop

### pkt2news.c Functionality
- Article number management (lines 42-69)
- Recursive directory creation (lines 72-110) - **DUPLICATES storage.c**
- Active file management (lines 113-175) - **MISSING from storage.c**
- USENET article storage with RFC1036 conversion (lines 178-259)
- Command-line argument parsing
- Main processing loop

### Current Storage Library Gaps
1. **Active file management** - Not implemented in storage.c
2. **Article numbering** - Placeholder implementation in storage.c
3. **Filename generation strategies** - Limited implementation
4. **Message duplicate detection** - Not implemented
5. **Path sanitization** - Basic implementation vs. comprehensive in utilities

## Implementation Plan

### Phase 1: Update Storage Library to Match Utility Behavior

#### 1.1 Active File Management (from pkt2news.c)
- **Function**: `ftn_storage_update_active_file()` - Complete implementation
- **Function**: `ftn_storage_get_next_article_number()` - Complete implementation
- **Location**: Update `src/storage.c` lines 691-695 (placeholders)
- **Behavior**: Match pkt2news.c lines 113-175 exactly
- **Format**: `newsgroup high low status` (e.g., `fidonet.general 123 1 y`)

#### 1.2 Advanced Filename Generation
- **Function**: `ftn_storage_generate_message_filename()` - New function
- **Location**: Add to `src/storage.c`
- **Behavior**: Match pkt2mail.c lines 162-221
- **Features**: MSGID-based names, timestamp fallback, sanitization

#### 1.3 Message Duplicate Detection
- **Function**: `ftn_storage_message_exists()` - New function
- **Location**: Add to `src/storage.c`
- **Behavior**: Match pkt2mail.c lines 224-279
- **Features**: Check new/, cur/, and flagged files

#### 1.4 Enhanced Path Sanitization
- **Function**: `ftn_storage_sanitize_filename()` - Update existing
- **Location**: Update `src/storage.c`
- **Behavior**: Match pkt2mail.c lines 42-55 character replacement

#### 1.5 Article Directory Structure
- **Behavior**: Ensure storage matches pkt2news.c lines 218-228
- **Format**: `USENET_ROOT/NETWORK/lowercase_area/article_number`
- **Integration**: Update `ftn_storage_store_news()` to match exactly

### Phase 2: Extend Storage Library Interface

#### 2.1 New Header Declarations
Add to `include/ftn/storage.h`:
```c
/* Advanced filename generation */
ftn_error_t ftn_storage_generate_message_filename(const ftn_message_t* msg, char** filename);

/* Message duplicate detection */
ftn_error_t ftn_storage_message_exists(ftn_storage_t* storage, const char* maildir_path,
                                      const ftn_message_t* msg, int* exists);

/* Utility-compatible storage functions */
ftn_error_t ftn_storage_store_mail_with_options(ftn_storage_t* storage, const ftn_message_t* msg,
                                               const char* maildir_path, const char* domain,
                                               const char* user_filter);

ftn_error_t ftn_storage_store_news_with_options(ftn_storage_t* storage, const ftn_message_t* msg,
                                               const char* usenet_root, const char* network);
```

#### 2.2 Configuration-Free Functions
- Add functions that work without full ftn_config_t setup
- Allow utilities to use storage functions with minimal config
- Support direct path specification

### Phase 3: Refactor pkt2news.c

#### 3.1 Remove Duplicate Functions
- **Delete**: `get_next_article_number()` (lines 42-69)
- **Delete**: `create_directory_recursive()` (lines 72-110)
- **Delete**: `update_active_file()` (lines 113-175)
- **Delete**: Most of `save_usenet_article()` (lines 178-259)

#### 3.2 Replace with Storage Library Calls
```c
/* Old: get_next_article_number() */
/* New: ftn_storage_get_next_article_number() */

/* Old: create_directory_recursive() */
/* New: ftn_storage_create_directory_recursive() */

/* Old: update_active_file() */
/* New: ftn_storage_update_active_file() */

/* Old: save_usenet_article() */
/* New: ftn_storage_store_news_with_options() */
```

#### 3.3 Simplified Main Function
- Keep argument parsing (lines 274-315)
- Replace packet processing with storage library calls
- Maintain exact same command-line interface and output format

### Phase 4: Refactor pkt2mail.c

#### 4.1 Remove Duplicate Functions
- **Delete**: `expand_user_path()` (lines 67-113)
- **Delete**: `create_maildir_structure()` (lines 116-159)
- **Delete**: `generate_filename()` (lines 162-221)
- **Delete**: `message_exists()` (lines 224-279)
- **Delete**: Most of `save_message_to_maildir()` (lines 282-370)

#### 4.2 Replace with Storage Library Calls
```c
/* Old: expand_user_path() */
/* New: ftn_storage_expand_path() */

/* Old: create_maildir_structure() */
/* New: ftn_storage_create_maildir() */

/* Old: generate_filename() */
/* New: ftn_storage_generate_message_filename() */

/* Old: message_exists() */
/* New: ftn_storage_message_exists() */

/* Old: save_message_to_maildir() */
/* New: ftn_storage_store_mail_with_options() */
```

#### 4.3 Simplified Main Function
- Keep argument parsing (lines 388-470)
- Replace message processing with storage library calls
- Maintain exact same command-line interface and output format

### Phase 5: Testing and Validation

#### 5.1 Behavioral Testing
- Test pkt2mail with existing packet files
- Test pkt2news with existing packet files
- Verify identical output directory structures
- Verify identical active file format
- Verify identical maildir structure

#### 5.2 Integration Testing
- Run storage library tests
- Test utilities with various command-line options
- Test error handling paths
- Test edge cases (empty packets, malformed messages)

#### 5.3 Performance Testing
- Ensure no performance regression
- Test with large packet files
- Test with many small files

## Files to Modify

### Storage Library
- `include/ftn/storage.h` - Add new function declarations
- `src/storage.c` - Implement missing functionality

### Utilities
- `src/pkt2mail.c` - Remove duplicated code, use storage library
- `src/pkt2news.c` - Remove duplicated code, use storage library

### Build System
- `Makefile` - Ensure dependencies are correct

### Tests
- `tests/test_storage.c` - Add tests for new functionality
- Create integration tests for utilities

## Success Criteria

1. **Code Reduction**: pkt2mail.c and pkt2news.c reduce from ~550 and ~360 lines to ~150-200 lines each
2. **Behavior Preservation**: Utilities produce identical output for same inputs
3. **API Consistency**: Storage library functions follow libFTN patterns
4. **Test Coverage**: All new storage functions have tests
5. **Documentation**: Functions are properly documented
6. **Performance**: No significant performance degradation
7. **Memory Safety**: No memory leaks or buffer overflows

## Implementation Priority

1. **High Priority**: Active file management and article numbering (needed for pkt2news)
2. **High Priority**: Advanced filename generation (needed for pkt2mail)
3. **Medium Priority**: Message duplicate detection
4. **Medium Priority**: Utility refactoring
5. **Low Priority**: Performance optimizations

## Risk Mitigation

1. **Backup Testing**: Keep original utilities during transition
2. **Incremental Changes**: Implement one function at a time
3. **Behavior Verification**: Test each change against original behavior
4. **Rollback Plan**: Git commits allow easy rollback if issues arise

This plan ensures that the storage consolidation maintains all existing functionality while significantly reducing code duplication and improving maintainability.