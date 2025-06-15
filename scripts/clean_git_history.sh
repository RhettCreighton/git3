#!/bin/bash
# Clean git history and start fresh with git3 PoW commits only

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== Git-Wizard Clean History Reset ===${NC}"
echo "This will:"
echo "1. Backup current state"
echo "2. Reset git history for both repos"
echo "3. Create fresh git3 repos with PoW"
echo "4. Sync clean git3 to fresh git"
echo ""
read -p "Continue? [y/N] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 1
fi

# Step 1: Backup current state
echo -e "\n${YELLOW}Step 1: Backing up current state...${NC}"
mkdir -p backup/$(date +%Y%m%d_%H%M%S)
cp -r .git backup/$(date +%Y%m%d_%H%M%S)/git_main_backup
cp -r .git3 backup/$(date +%Y%m%d_%H%M%S)/git3_main_backup
cp -r vendor/git3/.git backup/$(date +%Y%m%d_%H%M%S)/git_vendor_backup
cp -r vendor/git3/.git3 backup/$(date +%Y%m%d_%H%M%S)/git3_vendor_backup || true
echo -e "${GREEN}✓ Backup complete${NC}"

# Step 2: Save remote URLs
echo -e "\n${YELLOW}Step 2: Saving remote URLs...${NC}"
MAIN_REMOTE=$(git remote get-url origin 2>/dev/null || echo "")
cd vendor/git3
GIT3_REMOTE=$(git remote get-url origin 2>/dev/null || echo "")
cd ../..
echo "Main remote: $MAIN_REMOTE"
echo "Git3 remote: $GIT3_REMOTE"

# Step 3: Reset vendor/git3 first
echo -e "\n${YELLOW}Step 3: Resetting vendor/git3...${NC}"
cd vendor/git3

# Remove old git history
rm -rf .git
git init
if [ -n "$GIT3_REMOTE" ]; then
    git remote add origin "$GIT3_REMOTE"
fi

# Remove old git3 if exists
rm -rf .git3

# Initialize fresh git3
echo -e "${BLUE}Initializing fresh git3 repository...${NC}"
../../bin/git3 init

# Add all files for initial commit
../../bin/git3 add -A

# Create initial PoW commit
echo -e "${BLUE}Mining initial commit...${NC}"
../../bin/git3 commit -m "Initial commit: Git3 with SHA3-256 proof-of-work support"

# Create a clean commit
echo -e "${BLUE}Mining clean commit...${NC}"
../../bin/git3 commit --clean -m "Clean git3 codebase with optimized PoW mining"

# Check git3 log
echo -e "\n${GREEN}Git3 log for vendor/git3:${NC}"
../../bin/git3 log --oneline

# Get latest git3 commit info
GIT3_HEAD=$(../../bin/git3 rev-parse HEAD)
GIT3_WORK=$(../../bin/git3 log -1 | grep "Work:" | sed 's/Work: *//')
GIT3_TOTAL=$(../../bin/git3 log -1 | grep "Total:" | sed 's/Total: *//')

# Create git sync commit
echo -e "\n${BLUE}Creating git sync commit for vendor/git3...${NC}"
git add -A
git commit -m "[GIT3 SYNC] Clean git3 repository with PoW

Latest git3 commit: $GIT3_HEAD
Work: $GIT3_WORK
Total Chain Work: $GIT3_TOTAL

This is a clean git history containing only sync commits from git3.
All development happens in git3 with proof-of-work verification."

cd ../..

# Step 4: Reset main git-wizard repo
echo -e "\n${YELLOW}Step 4: Resetting main git-wizard repository...${NC}"

# Save important files
mkdir -p temp_save
cp -r src temp_save/
cp -r include temp_save/
cp -r bin temp_save/
cp -r vendor/sha3 temp_save/
cp *.md temp_save/
cp .gitignore temp_save/
cp CMakeLists.txt temp_save/
cp -r .git/hooks temp_save/

# Remove old git
rm -rf .git
git init
if [ -n "$MAIN_REMOTE" ]; then
    git remote add origin "$MAIN_REMOTE"
fi

# Restore hooks
cp -r temp_save/hooks .git/

# Remove old git3
rm -rf .git3

# Initialize fresh git3
echo -e "${BLUE}Initializing fresh git3 repository...${NC}"
./bin/git3 init

# Restore files
cp -r temp_save/* .
rm -rf temp_save

# Add vendor/git3 as submodule
git submodule add "$GIT3_REMOTE" vendor/git3 || true
cd vendor/git3
git checkout main
cd ../..

# Add all files for initial commit
./bin/git3 add -A

# Create initial PoW commit
echo -e "${BLUE}Mining initial commit for main repo...${NC}"
./bin/git3 commit -m "Initial commit: Git-wizard with SHA3-256 proof-of-work"

# Create a clean commit
echo -e "${BLUE}Mining clean commit for main repo...${NC}"
./bin/git3 commit --clean -m "Clean git-wizard codebase with fast PoW mining"

# Check git3 log
echo -e "\n${GREEN}Git3 log for main repo:${NC}"
./bin/git3 log --oneline

# Get latest git3 commit info
MAIN_GIT3_HEAD=$(./bin/git3 rev-parse HEAD)
MAIN_GIT3_WORK=$(./bin/git3 log -1 | grep "Work:" | sed 's/Work: *//')
MAIN_GIT3_TOTAL=$(./bin/git3 log -1 | grep "Total:" | sed 's/Total: *//')

# Create git sync commit
echo -e "\n${BLUE}Creating git sync commit for main repo...${NC}"
git add -A
git commit -m "[GIT3 SYNC] Clean git-wizard repository with PoW

Latest git3 commit: $MAIN_GIT3_HEAD
Work: $MAIN_GIT3_WORK
Total Chain Work: $MAIN_GIT3_TOTAL

This is a clean git history containing only sync commits from git3.
All development happens in git3 with proof-of-work verification.
Submodules are also clean with their own git3 PoW chains."

# Step 5: Summary
echo -e "\n${GREEN}=== Clean History Reset Complete ===${NC}"
echo ""
echo "Main repository:"
echo "  Git3 HEAD: $MAIN_GIT3_HEAD"
echo "  Git HEAD: $(git rev-parse HEAD)"
echo "  Total Work: $MAIN_GIT3_TOTAL"
echo ""
echo "Vendor/git3 submodule:"
echo "  Git3 HEAD: $GIT3_HEAD"
echo "  Git HEAD: $(cd vendor/git3 && git rev-parse HEAD)"
echo "  Total Work: $GIT3_TOTAL"
echo ""
echo -e "${YELLOW}Ready to push clean history:${NC}"
echo "  Main repo: git push -f origin main"
echo "  Git3 submodule: cd vendor/git3 && git push -f origin main"
echo ""
echo -e "${RED}WARNING: This will force-push and overwrite remote history!${NC}"