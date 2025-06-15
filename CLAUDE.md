# Git3 Development Guide

## Overview

Git3 is a modified version of Git that uses SHA3-256 hashing and requires proof-of-work for all commits. This provides blockchain-like security properties while maintaining full Git compatibility.

## Critical Implementation Details

### 1. SHA3-256 Integration
- **Hash Algorithm**: All object hashing uses SHA3-256 (Keccak)
- **Object Format**: Compatible with Git's object format but with SHA3-256 hashes
- **Repository Version**: Git3 repositories use `repositoryformatversion = 3`
- **Extension**: `extensions.objectformat = sha3`

### 2. Proof-of-Work System
- **Every commit requires PoW**: Minimum 20 bits of leading zeros (1M+ hashes)
- **Mining happens before object creation**: Critical for performance
- **Cumulative work tracking**: Each commit stores parent's cumulative work
- **Poisson process**: Mining follows exponential distribution

### 3. Key Source Files

#### Core PoW Implementation
- `pow.h` - Proof-of-work data structures and function declarations
- `pow.c` - Basic PoW implementation with statistical display
- `pow_optimized.c` - Optimized mining (currently used)
- `tag_with_pow.c` - PoW support for tags

#### SHA3 Integration
- `sha3/` - SHA3-256 (Keccak) implementation
- `hash.h` - Modified to support SHA3
- `object-file.c` - Uses SHA3 for object hashing

#### Modified Git Components
- `environment.h` - Changed `DEFAULT_GIT_DIR_ENVIRONMENT` to `.git3`
- `setup.c` - Modified to look for `.git3` directories
- `path.c` - Updated path discovery for `.git3`
- `dir.c` - Ignores `.git3` directories in working tree
- `builtin/init-db.c` - Forces SHA3 and version 3 for new repos

### 4. Build System
```bash
make clean
make prefix=/home/bob/local-bin
make prefix=/home/bob/local-bin install
```

### 5. Dual Repository Management

This directory (`vendor/git3`) maintains:
- `.git/` - Regular Git repository for GitHub (SHA1)
- `.git3/` - Git3 repository for development (SHA3 + PoW)

**Important**: The `.gitignore` must contain `.git3/` to prevent Git from tracking Git3 files.

### 6. Commit Standards

#### In `.git` (Regular Git)
All commits MUST start with `[GIT3 SYNC]` prefix. This is enforced by git-wizard.

#### In `.git3` (Git3)
Normal development commits with proof-of-work. Each commit shows:
```
✓ Found valid PoW hash: 00000abc...
  Difficulty: 20 bits (required: 20)
  Work: 1.0 MH (2^20)
  Cumulative: 15.3 MH
  Nonce: 1234567
  Expected: 1.0 MH ± 1 KH (σ=1 KH)
```

### 7. Testing

Run from git-wizard root:
```bash
./test_git_wizard.sh  # Comprehensive tests
./test_boolean.sh     # Simple pass/fail
./check_clean_state.sh # Verify clean state
```

### 8. Key Differences from Standard Git

1. **Directory**: Uses `.git3` instead of `.git`
2. **Hashing**: SHA3-256 instead of SHA1/SHA256
3. **Commits**: Require proof-of-work mining
4. **Version**: Repository format version 3
5. **Security**: Blockchain-like tamper resistance

### 9. Performance Optimizations

- Mining optimized to ~1.9M hashes/second
- Average commit time < 1 second
- Parallel mining support (future)
- AVX2/AVX512 optimizations (future)

### 10. Development Workflow

1. Make changes in this directory
2. Test with: `./git3 --version`
3. Commit to git3: `git3 add . && git3 commit -m "message"`
4. Sync to git: Use git-wizard from parent directory
5. Push to GitHub: `git push origin main`

### 11. Important Files to Never Modify

- `.git3/` - This directory is local-only
- Object files (*.o) - Build artifacts
- Compiled binaries - Except for final git3 binary

### 12. C99 and CMake Standards

This project follows:
- C99 standard
- CMake build system
- No Python dependencies
- No shell scripts where C binaries can be used
- High code quality standards
- Comprehensive testing

## Remember

Git3 is designed to be a "superhero" version of Git. Every feature should enhance security without sacrificing usability. The proof-of-work system provides cryptographic timestamps and tamper-evidence while keeping the familiar Git workflow.