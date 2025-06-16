#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "pow.h"
#include "hash.h"
#include "hex.h"
#include "object.h"
#include "commit.h"
#include "strbuf.h"
#include "repository.h"
#include "object-file.h"
#include "object-store.h"
#include <signal.h>

/* Global flag for interrupt handling */
static volatile sig_atomic_t mining_interrupted = 0;

/* Signal handler for interrupt */
static void handle_interrupt(int sig)
{
    mining_interrupted = 1;
    printf("\n\nMining interrupted by user (Ctrl+C)...\n");
}

/* Build commit object format in memory for fast mining */
static int build_commit_for_mining(struct strbuf *commit_buf,
                                  const struct object_id *tree_oid,
                                  const struct object_id *parent_oid,
                                  const char *author,
                                  const char *committer,
                                  const char *message,
                                  enum commit_type type,
                                  uint32_t difficulty,
                                  uint64_t parent_cumulative_work,
                                  size_t *nonce_offset_out)
{
    /* Clear buffer */
    strbuf_reset(commit_buf);
    strbuf_grow(commit_buf, 8192);
    
    /* Add tree */
    strbuf_addf(commit_buf, "tree %s\n", oid_to_hex(tree_oid));
    
    /* Add parent if exists */
    if (parent_oid) {
        strbuf_addf(commit_buf, "parent %s\n", oid_to_hex(parent_oid));
    }
    
    /* Add author and committer */
    strbuf_addf(commit_buf, "author %s\n", author);
    strbuf_addf(commit_buf, "committer %s\n", committer);
    
    /* Empty line before message */
    strbuf_addch(commit_buf, '\n');
    
    /* Add commit type prefix if not normal */
    if (type == COMMIT_TYPE_FREEZE) {
        strbuf_addstr(commit_buf, "[FREEZE] ");
    } else if (type == COMMIT_TYPE_CLEAN) {
        strbuf_addstr(commit_buf, "[CLEAN] ");
    }
    
    /* Add message */
    strbuf_addstr(commit_buf, message);
    
    /* Add PoW metadata */
    strbuf_addf(commit_buf, "\n\n");
    strbuf_addf(commit_buf, "PoW-Nonce: ");
    
    /* Record where nonce will be inserted */
    *nonce_offset_out = commit_buf->len;
    
    /* Reserve space for nonce (up to 20 digits) */
    strbuf_addstr(commit_buf, "00000000000000000000");
    
    strbuf_addf(commit_buf, "\nPoW-Difficulty: %u\n", difficulty);
    strbuf_addf(commit_buf, "PoW-Parent-Work: %lu", parent_cumulative_work);
    
    return 0;
}

/* Optimized mining function that hashes raw commit data */
int mine_pow_commit_optimized(const struct object_id *tree_oid,
                             const struct object_id *parent_oid,
                             const char *author,
                             const char *committer,
                             const char *message,
                             enum commit_type type,
                             uint32_t difficulty,
                             struct object_id *result_oid,
                             struct pow_data *pow_out)
{
    struct strbuf commit_buf = STRBUF_INIT;
    size_t nonce_offset;
    uint64_t nonce = 0;
    int ret = -1;
    
    const char *type_names[] = {"", "[FREEZE] ", "[CLEAN] "};
    const char *type_name = type_names[type];
    
    printf("Mining %sproof-of-work commit (difficulty: %u bits)...\n", type_name, difficulty);
    
    /* Set up interrupt handler */
    struct sigaction sa, old_sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_interrupt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    mining_interrupted = 0;
    sigaction(SIGINT, &sa, &old_sa);
    
    /* Calculate parent's cumulative work */
    uint64_t parent_cumulative_work = 0;
    if (parent_oid) {
        parent_cumulative_work = calculate_total_work(parent_oid);
        if (parent_cumulative_work > 0) {
            char work_str[32];
            format_work(parent_cumulative_work, work_str, sizeof(work_str));
            printf("Parent cumulative work: %s\n", work_str);
        }
    }
    
    /* Build commit template */
    if (build_commit_for_mining(&commit_buf, tree_oid, parent_oid,
                               author, committer, message, type,
                               difficulty, parent_cumulative_work,
                               &nonce_offset) < 0) {
        goto cleanup;
    }
    
    /* Mining loop - only update nonce and hash */
    while (1) {
        /* Check for interrupt */
        if (mining_interrupted) {
            printf("Mining cancelled.\n");
            ret = -1;
            goto cleanup;
        }
        
        /* Update nonce in buffer */
        char nonce_str[21];
        int nonce_len = snprintf(nonce_str, sizeof(nonce_str), "%lu", nonce);
        
        /* Overwrite the nonce portion */
        memcpy(commit_buf.buf + nonce_offset, nonce_str, nonce_len);
        
        /* Calculate length up to end of nonce */
        size_t content_len = nonce_offset + nonce_len;
        
        /* Add the rest after nonce */
        char temp[256];
        snprintf(temp, sizeof(temp), "\nPoW-Difficulty: %u\nPoW-Parent-Work: %lu", 
                 difficulty, parent_cumulative_work);
        
        /* Temporarily terminate and restore */
        char saved = commit_buf.buf[content_len];
        commit_buf.buf[content_len] = '\0';
        
        /* Build final content */
        struct strbuf final_buf = STRBUF_INIT;
        strbuf_addstr(&final_buf, commit_buf.buf);
        strbuf_addstr(&final_buf, temp);
        
        /* Hash the commit object */
        hash_object_file(the_hash_algo, final_buf.buf, final_buf.len, 
                        OBJ_COMMIT, result_oid);
        
        /* Check if hash meets difficulty */
        char hex[GIT_MAX_HEXSZ + 1];
        oid_to_hex_r(hex, result_oid);
        
        if (nonce < 10 || nonce % 100000 == 0) {
            printf("  Mining... (nonce: %lu, hash: %s)\n", nonce, hex);
        }
        
        if (check_pow_difficulty(hex, difficulty)) {
            uint64_t this_work = calculate_hash_work(hex);
            uint64_t total_work = parent_cumulative_work + this_work;
            
            char this_work_str[32], total_work_str[32];
            format_work(this_work, this_work_str, sizeof(this_work_str));
            format_work(total_work, total_work_str, sizeof(total_work_str));
            
            /* Calculate actual bits of zeros */
            uint32_t actual_bits = 0;
            for (size_t i = 0; i < strlen(hex); i++) {
                char c = hex[i];
                if (c == '0') {
                    actual_bits += 4;
                } else if (c == '1') {
                    actual_bits += 3;
                    break;
                } else if (c >= '2' && c <= '3') {
                    actual_bits += 2;
                    break;
                } else if (c >= '4' && c <= '7') {
                    actual_bits += 1;
                    break;
                } else {
                    break;
                }
            }
            
            printf("\n✓ Found valid PoW hash: %s\n", hex);
            printf("  Difficulty: %u bits (required: %u)\n", actual_bits, difficulty);
            printf("  Work: %s (2^%u)\n", this_work_str, actual_bits);
            printf("  Cumulative: %s\n", total_work_str);
            printf("  Nonce: %lu\n", nonce);
            
            /* Now create the actual commit object */
            struct commit_list *parents = NULL;
            if (parent_oid) {
                struct commit *parent = lookup_commit(the_repository, parent_oid);
                if (parent) {
                    commit_list_insert(parent, &parents);
                }
            }
            
            /* Build final message with PoW metadata */
            struct strbuf final_message = STRBUF_INIT;
            if (type == COMMIT_TYPE_FREEZE) {
                strbuf_addstr(&final_message, "[FREEZE] ");
            } else if (type == COMMIT_TYPE_CLEAN) {
                strbuf_addstr(&final_message, "[CLEAN] ");
            }
            strbuf_addstr(&final_message, message);
            strbuf_addf(&final_message, "\n\nPoW-Nonce: %lu\n", nonce);
            strbuf_addf(&final_message, "PoW-Difficulty: %u\n", difficulty);
            strbuf_addf(&final_message, "PoW-Parent-Work: %lu", parent_cumulative_work);
            
            /* Create the commit object */
            if (commit_tree_extended(final_message.buf, final_message.len,
                                    tree_oid, parents,
                                    result_oid, author, committer,
                                    NULL, NULL) < 0) {
                free_commit_list(parents);
                strbuf_release(&final_message);
                strbuf_release(&final_buf);
                goto cleanup;
            }
            
            free_commit_list(parents);
            strbuf_release(&final_message);
            
            if (pow_out) {
                pow_out->nonce = nonce;
                pow_out->difficulty = difficulty;
                pow_out->work = this_work;
                pow_out->cumulative_work = total_work;
            }
            
            ret = 0;
            strbuf_release(&final_buf);
            goto cleanup;
        }
        
        strbuf_release(&final_buf);
        commit_buf.buf[content_len] = saved;
        nonce++;
    }
    
cleanup:
    /* Restore original signal handler */
    sigaction(SIGINT, &old_sa, NULL);
    strbuf_release(&commit_buf);
    return ret;
}