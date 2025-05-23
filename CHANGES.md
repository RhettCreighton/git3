# Git3 Changes Summary

This document summarizes all changes made to convert Git to Git3 with SHA3-256 only support.

## New Files Added

1. **SHA3 Implementation**
   - `sha3/block/sha3.h` - SHA3-256 header file
   - `sha3/block/sha3.c` - SHA3-256 implementation (FIPS 202)
   - Copyright (C) 2025 Rhett Creighton, GPL v2

## Modified Files

### Core Hash Infrastructure

1. **hash.h**
   - Removed all SHA1 and SHA256 backend includes
   - Added SHA3 backend include
   - Changed constants:
     - `GIT_HASH_SHA1` → `GIT_HASH_SHA3`
     - `GIT_SHA1_FORMAT_ID` → `GIT_SHA3_FORMAT_ID` (0x73686133)
     - `GIT_SHA1_RAWSZ` → `GIT_SHA3_RAWSZ` (32)
     - `GIT_SHA1_HEXSZ` → `GIT_SHA3_HEXSZ` (64)
     - `GIT_SHA1_BLKSZ` → `GIT_SHA3_BLKSZ` (136)
   - Removed SHA1/SHA256 specific code
   - Updated hash context union to only include SHA3

2. **hash.c**
   - Removed SHA1 and SHA256 implementations
   - Added SHA3 implementation functions
   - Updated hash_algos array to only include SHA3
   - Added SHA3-256 empty tree and blob OIDs

3. **setup.h**
   - Changed `REPOSITORY_FORMAT_INIT` default from `GIT_HASH_SHA1` to `GIT_HASH_SHA3`

### Configuration and Initialization

4. **builtin/init-db.c**
   - Force SHA3 as only supported algorithm
   - Reject any other object format

5. **setup.c**
   - Updated to only accept SHA3 in configuration
   - Modified hash algorithm validation

### Build System

6. **Makefile**
   - Changed binary name from `git` to `git3`
   - Changed gitexecdir from `libexec/git-core` to `libexec/git3-core`
   - Added SHA3 source files to build

7. **meson.build**
   - Updated to include SHA3 implementation

### Supporting Files

8. **Various .c files** (automatic replacements)
   - Replaced `GIT_HASH_SHA1` with `GIT_HASH_SHA3`
   - Replaced `GIT_HASH_SHA256` with `GIT_HASH_SHA3`
   - Replaced `GIT_SHA1_RAWSZ` with `GIT_SHA3_RAWSZ`
   - Replaced `GIT_SHA256_RAWSZ` with `GIT_SHA3_RAWSZ`

9. **help.c**
   - Updated version info to show SHA3-256 backend

10. **bundle.c**
    - Updated default hash algorithm to SHA3

11. **chunk-format.c**
    - Updated OID version handling for SHA3

12. **refs/reftable-backend.c**
    - Updated to handle SHA3 format ID

## Documentation

13. **README.md**
    - Completely rewritten to document Git3
    - Explains SHA3-only support
    - Provides installation and usage instructions
    - Warns about incompatibility with standard Git

## Key Changes Summary

- **Binary Name**: `git` → `git3`
- **Hash Algorithm**: SHA1/SHA256 → SHA3-256 only
- **Hash Length**: 40/64 chars → 64 chars only
- **Installation Path**: `libexec/git-core` → `libexec/git3-core`
- **Compatibility**: Not compatible with standard Git repositories

All changes maintain GPL v2 licensing consistent with the original Git project.