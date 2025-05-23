# Git3 - Git with SHA3-256 Only

Git3 is a modified version of Git that uses **SHA3-256 as the only supported hash algorithm**. This is a hard fork that removes support for SHA1 and SHA256, making SHA3-256 mandatory for all operations.

## Key Differences from Standard Git

### Hash Algorithm
- **Standard Git**: Supports SHA1 (default) and SHA256 (via `--object-format=sha256`)
- **Git3**: Supports **ONLY SHA3-256** - no other hash algorithms are available

### Hash Characteristics
- **Algorithm**: SHA3-256 (Keccak-256, NIST FIPS 202)
- **Hash Length**: 256 bits (32 bytes)
- **Hex Display**: 64 characters
- **Block Size**: 136 bytes (1088 bits)

### Compatibility
⚠️ **CRITICAL**: Git3 repositories are **NOT compatible** with standard Git:
- Git3 can only work with SHA3-256 repositories
- Standard Git cannot read Git3 repositories
- You cannot push/pull between Git and Git3 repositories
- You cannot convert existing SHA1/SHA256 repositories to SHA3

## Installation

### From Source

```bash
# Clone the repository
git clone https://github.com/RhettCreighton/git3.git
cd git3

# Build
make -j$(nproc)

# Install system-wide (requires root)
sudo make install

# OR install to home directory (no root required)
make prefix=$HOME/.local install
# Then add to PATH:
export PATH="$HOME/.local/bin:$PATH"
```

### Verify Installation

```bash
# Check version
git3 --version

# Verify SHA3 support (will create a test repo)
git3 init test-repo
cd test-repo
echo "test" > file.txt
git3 add file.txt
git3 commit -m "Test commit"
git3 rev-parse HEAD  # Should show 64-character hash
```

## Usage

Git3 commands are **identical** to Git commands, but all operations use SHA3-256:

```bash
# Initialize a new repository
git3 init

# Clone a repository (only works with other git3 repos)
git3 clone https://github.com/user/git3-repo.git

# Standard workflow
git3 add .
git3 commit -m "Your message"
git3 branch feature
git3 checkout feature
git3 merge main
git3 push origin main
git3 pull origin main

# All standard Git commands work
git3 log
git3 status
git3 diff
git3 rebase
git3 cherry-pick
git3 tag v1.0.0
```

## Technical Details

### SHA3-256 Implementation
- Pure C implementation based on FIPS 202 specification
- Located in `sha3/block/sha3.c` and `sha3/block/sha3.h`
- Integrated into Git's hash infrastructure
- Copyright (C) 2025 Rhett Creighton, Licensed under GPL v2

### Object Format
- Default and only format: SHA3-256
- Format ID: `0x73686133` ("sha3" in hex)
- Cannot be changed via `--object-format` (SHA3 is hardcoded)

### Modified Components
1. **Hash Infrastructure** (`hash.h`, `hash.c`)
   - Removed SHA1 and SHA256 support
   - Added SHA3-256 as the only algorithm
   - Constants: `GIT_HASH_SHA3`, `GIT_SHA3_RAWSZ` (32), `GIT_SHA3_HEXSZ` (64)

2. **Initialization** (`builtin/init-db.c`, `setup.c`)
   - Forces SHA3 as the only option
   - Rejects any attempt to use other algorithms

3. **Build System** (`Makefile`, `meson.build`)
   - Builds as `git3` binary
   - Installs to `libexec/git3-core`
   - All git commands available as git3 commands

### Example Output

```bash
$ git3 init
Initialized empty Git repository in /home/user/project/.git/

$ echo "Hello SHA3" > README.md
$ git3 add README.md
$ git3 commit -m "Initial commit"
[main (root-commit) a7f9c8e3b5d2145f6890abcdef1234567890abcdef1234567890abcdef123456] Initial commit
 1 file changed, 1 insertion(+)
 create mode 100644 README.md

$ git3 log --oneline
a7f9c8e3b5d2145f6890abcdef1234567890abcdef1234567890abcdef123456 Initial commit

$ git3 cat-file -p HEAD
tree 5d8f6b9c3a2e1470abcdef1234567890abcdef1234567890abcdef1234567890
author User Name <user@example.com> 1234567890 +0000
committer User Name <user@example.com> 1234567890 +0000

Initial commit
```

## Migration and Compatibility

Since Git3 is incompatible with standard Git:

1. **New Projects**: Can start directly with Git3
2. **Existing Projects**: Cannot be converted - you must start fresh
3. **Collaboration**: All team members must use Git3
4. **Hosting**: Requires Git3-aware hosting (standard Git hosts won't work)

## Why SHA3-256?

- **Latest Standard**: SHA3 is the newest NIST-approved hash standard
- **Different Design**: Based on Keccak sponge construction, not Merkle-Damgård
- **Future Proof**: Designed to complement SHA2, not replace it
- **Drop-in Replacement**: Same output size as SHA256, making integration easier

## Documentation

Most Git documentation applies to Git3, with the key difference being the hash algorithm.
See [Documentation/gittutorial.adoc][] to get started.

## Building from Source

### Prerequisites
- GCC or Clang
- GNU Make
- zlib development files
- Optional: OpenSSL, libcurl for HTTPS support

### Build Options
```bash
# Standard build
make

# Parallel build
make -j$(nproc)

# Debug build
make DEBUG=1

# Custom installation prefix
make prefix=/opt/git3
make prefix=/opt/git3 install
```

## Contributing

This is a fork of the official Git project. The SHA3 modifications are:
- Copyright (C) 2025 Rhett Creighton
- Licensed under GPL v2 (same as Git)

When contributing:
1. Ensure SHA3-only operation is maintained
2. No code that supports SHA1/SHA256 should be added
3. Test with SHA3 repositories only
4. Submit issues and pull requests to https://github.com/RhettCreighton/git3

## Original Git Information

Git3 is based on Git, which is an Open Source project covered by the GNU General Public
License version 2. It was originally written by Linus Torvalds with help of a group of
hackers around the net.

For information about the original Git project, see https://git-scm.com/

[INSTALL]: INSTALL
[Documentation/gittutorial.adoc]: Documentation/gittutorial.adoc
[Documentation/giteveryday.adoc]: Documentation/giteveryday.adoc
[Documentation/gitcvs-migration.adoc]: Documentation/gitcvs-migration.adoc
[Documentation/SubmittingPatches]: Documentation/SubmittingPatches
[Documentation/CodingGuidelines]: Documentation/CodingGuidelines
[po/README.md]: po/README.md
