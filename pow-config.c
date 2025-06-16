#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "pow.h"
#include "config.h"
#include "refs.h"
#include "repository.h"
#include "strbuf.h"

/* Default difficulty levels for different branch types */
#define POW_DIFFICULTY_DEV 8
#define POW_DIFFICULTY_FEATURE 10
#define POW_DIFFICULTY_MAIN 12
#define POW_DIFFICULTY_RELEASE 16

/* Get configured difficulty for a specific branch pattern */
static int get_branch_pattern_difficulty(const char *branch)
{
    struct strbuf key = STRBUF_INIT;
    int difficulty = -1;
    
    /* Check for exact branch config first */
    strbuf_addf(&key, "branch.%s.powdifficulty", branch);
    git_config_get_int(key.buf, &difficulty);
    
    if (difficulty > 0) {
        strbuf_release(&key);
        return difficulty;
    }
    
    /* Check for pattern-based config */
    strbuf_reset(&key);
    
    /* Development branches */
    if (starts_with(branch, "dev/") || starts_with(branch, "develop/")) {
        strbuf_addstr(&key, "pow.difficulty.dev");
        if (git_config_get_int(key.buf, &difficulty) == 0 && difficulty > 0) {
            strbuf_release(&key);
            return difficulty;
        }
        strbuf_release(&key);
        return POW_DIFFICULTY_DEV;
    }
    
    /* Feature branches */
    if (starts_with(branch, "feature/") || starts_with(branch, "feat/")) {
        strbuf_reset(&key);
        strbuf_addstr(&key, "pow.difficulty.feature");
        if (git_config_get_int(key.buf, &difficulty) == 0 && difficulty > 0) {
            strbuf_release(&key);
            return difficulty;
        }
        strbuf_release(&key);
        return POW_DIFFICULTY_FEATURE;
    }
    
    /* Release branches */
    if (starts_with(branch, "release/") || starts_with(branch, "v")) {
        strbuf_reset(&key);
        strbuf_addstr(&key, "pow.difficulty.release");
        if (git_config_get_int(key.buf, &difficulty) == 0 && difficulty > 0) {
            strbuf_release(&key);
            return difficulty;
        }
        strbuf_release(&key);
        return POW_DIFFICULTY_RELEASE;
    }
    
    /* Main/master branches */
    if (strcmp(branch, "main") == 0 || strcmp(branch, "master") == 0) {
        strbuf_reset(&key);
        strbuf_addstr(&key, "pow.difficulty.main");
        if (git_config_get_int(key.buf, &difficulty) == 0 && difficulty > 0) {
            strbuf_release(&key);
            return difficulty;
        }
        strbuf_release(&key);
        return POW_DIFFICULTY_MAIN;
    }
    
    /* Default difficulty */
    strbuf_reset(&key);
    strbuf_addstr(&key, "pow.difficulty.default");
    if (git_config_get_int(key.buf, &difficulty) == 0 && difficulty > 0) {
        strbuf_release(&key);
        return difficulty;
    }
    
    strbuf_release(&key);
    return POW_DIFFICULTY_FEATURE;
}

/* Get the appropriate PoW difficulty for the current branch */
int get_pow_difficulty_for_branch(void)
{
    struct strbuf branch = STRBUF_INIT;
    int difficulty;
    
    /* Get current branch name */
    const char *head = refs_resolve_ref_unsafe(get_main_ref_store(the_repository),
                                              "HEAD", 0, NULL, NULL);
    if (!head || !starts_with(head, "refs/heads/")) {
        /* Not on a branch, use default */
        return GIT3_MIN_DIFFICULTY;
    }
    
    strbuf_addstr(&branch, head + strlen("refs/heads/"));
    difficulty = get_branch_pattern_difficulty(branch.buf);
    strbuf_release(&branch);
    
    /* Ensure minimum difficulty */
    if (difficulty < GIT3_MIN_DIFFICULTY) {
        difficulty = GIT3_MIN_DIFFICULTY;
    }
    
    return difficulty;
}

/* Set difficulty for a branch pattern in config */
int set_pow_difficulty_config(const char *pattern, int difficulty)
{
    struct strbuf key = STRBUF_INIT;
    int ret;
    
    if (difficulty < 1 || difficulty > 256) {
        return -1;
    }
    
    strbuf_addf(&key, "pow.difficulty.%s", pattern);
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%d", difficulty);
    ret = git_config_set_gently(key.buf, value_str);
    strbuf_release(&key);
    
    return ret;
}