# Git3 Sync Protocol

This repository follows the Git3 Sync Protocol where:

1. All development happens in git3 first (with SHA3-256 and proof-of-work)
2. Changes are synced to git with commit messages that include the git3 hash
3. Format: `[GIT3 SYNC] SHA3 <git3-hash> <description>`

## Example

```
[GIT3 SYNC] SHA3 00000a440b4f2996243ca26083bf201b1683eee43c663bca25e73535ea776ef0 Add documentation
```

This ensures full traceability between git and git3 repositories.