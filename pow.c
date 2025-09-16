#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "pow.h"
#include "pow_avx2.h"
#include "hash.h"
#include "hex.h"
#include "object.h"
#include "commit.h"
#include "strbuf.h"
#include "repository.h"
#include "object-file.h"
#include "object-store.h"

uint64_t calculate_hash_work(const char *hash_hex)
{
    uint32_t leading_zeros = 0;
    size_t i;
    
    for (i = 0; i < strlen(hash_hex); i++) {
        char c = hash_hex[i];
        if (c == '0') {
            leading_zeros += 4;
        } else if (c == '1') {
            leading_zeros += 3;
            break;
        } else if (c >= '2' && c <= '3') {
            leading_zeros += 2;
            break;
        } else if (c >= '4' && c <= '7') {
            leading_zeros += 1;
            break;
        } else {
            break;
        }
    }
    
    /* Work is 2^difficulty */
    if (leading_zeros == 0)
        return 1;
    return (uint64_t)1 << leading_zeros;
}

uint64_t calculate_total_work(const struct object_id *commit_oid)
{
    struct commit *commit;
    uint64_t total_work = 0;
    char hex[GIT_MAX_HEXSZ + 1];
    
    /* Get commit object */
    commit = lookup_commit(the_repository, commit_oid);
    if (!commit || repo_parse_commit(the_repository, commit) < 0)
        return 0;
    
    /* Calculate work for this commit */
    oid_to_hex_r(hex, commit_oid);
    total_work = calculate_hash_work(hex);
    
    /* Add parent's work recursively */
    if (commit->parents) {
        struct commit *parent = commit->parents->item;
        total_work += calculate_total_work(&parent->object.oid);
    }
    
    return total_work;
}

void format_work(uint64_t work, char *buffer, size_t size)
{
    if (work < 1000) {
        snprintf(buffer, size, "%lu", work);
    } else if (work < 1000000) {
        snprintf(buffer, size, "%.1fK", work / 1000.0);
    } else if (work < 1000000000) {
        snprintf(buffer, size, "%.1fM", work / 1000000.0);
    } else if (work < 1000000000000ULL) {
        snprintf(buffer, size, "%.1fB", work / 1000000000.0);
    } else {
        snprintf(buffer, size, "%.1fT", work / 1000000000000.0);
    }
}

int check_pow_difficulty(const char *hash_hex, uint32_t difficulty)
{
    uint32_t zero_bits = 0;
    size_t i;
    
    for (i = 0; i < strlen(hash_hex); i++) {
        char c = hash_hex[i];
        if (c >= 'A' && c <= 'F')
            c = c - 'A' + 'a';
        uint8_t nibble;
        
        if (c >= '0' && c <= '9') {
            nibble = (uint8_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            nibble = (uint8_t)(10 + (c - 'a'));
        } else {
            return 0;
        }
        
        /* Count leading zeros in this nibble */
        if (nibble == 0) {
            zero_bits += 4;
        } else if (nibble < 2) {
            zero_bits += 3;
            break;
        } else if (nibble < 4) {
            zero_bits += 2;
            break;
        } else if (nibble < 8) {
            zero_bits += 1;
            break;
        } else {
            break;
        }
    }
    
    return zero_bits >= difficulty;
}

int mine_pow_commit(const struct object_id *tree_oid,
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
    struct strbuf header_buf = STRBUF_INIT;
    uint64_t nonce = 0;
    int ret = -1;
    char hex[GIT_MAX_HEXSZ + 1];
    
    const char *type_names[] = {"", "[FREEZE] ", "[CLEAN] "};
    const char *type_name = type_names[type];
    
    printf("Mining %sproof-of-work commit (difficulty: %u bits)...\n", type_name, difficulty);
    
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
    
    /* Build the fixed part of the commit object */
    strbuf_addf(&header_buf, "tree %s\n", oid_to_hex(tree_oid));
    if (parent_oid) {
        strbuf_addf(&header_buf, "parent %s\n", oid_to_hex(parent_oid));
    }
    strbuf_addf(&header_buf, "author %s\n", author);
    strbuf_addf(&header_buf, "committer %s\n", committer);
    strbuf_addch(&header_buf, '\n');
    
    /* Mining loop - build commit object and hash directly */
    while (1) {
        /* Reset commit buffer */
        strbuf_reset(&commit_buf);
        
        /* Add header */
        strbuf_addbuf(&commit_buf, &header_buf);
        
        /* Add commit type prefix if not normal */
        if (type == COMMIT_TYPE_FREEZE) {
            strbuf_addstr(&commit_buf, "[FREEZE] ");
        } else if (type == COMMIT_TYPE_CLEAN) {
            strbuf_addstr(&commit_buf, "[CLEAN] ");
        }
        
        /* Add message and PoW metadata */
        strbuf_addstr(&commit_buf, message);
        strbuf_addf(&commit_buf, "\n\nPoW-Nonce: %lu\n", nonce);
        strbuf_addf(&commit_buf, "PoW-Difficulty: %u\n", difficulty);
        strbuf_addf(&commit_buf, "PoW-Parent-Work: %lu", parent_cumulative_work);
        
        /* Hash the commit object directly */
        hash_object_file(the_hash_algo, commit_buf.buf, commit_buf.len, 
                        OBJ_COMMIT, result_oid);
        
        /* Check if hash meets difficulty */
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
            
            /* Now write the object to storage */
            if (write_object_file(commit_buf.buf, commit_buf.len, OBJ_COMMIT, result_oid) < 0) {
                error("Failed to write commit object");
                goto cleanup;
            }
            
            if (pow_out) {
                pow_out->nonce = nonce;
                pow_out->difficulty = difficulty;
                pow_out->work = this_work;
                pow_out->cumulative_work = total_work;
            }
            
            ret = 0;
            goto cleanup;
        }
        
        nonce++;
    }
    
cleanup:
    strbuf_release(&commit_buf);
    strbuf_release(&header_buf);
    return ret;
}

int mine_pow_tag(const struct object_id *object_oid,
                 const char *type,
                 const char *tag,
                 const char *tagger,
                 const char *message,
                 const char *tag_type,
                 uint32_t difficulty,
                 struct object_id *result_oid,
                 struct pow_data *pow_out)
{
    struct strbuf tag_buf = STRBUF_INIT;
    struct object_id tag_oid;
    char hex[GIT_MAX_HEXSZ + 1];
    uint64_t nonce = 0;
    
    /* Ensure minimum difficulty */
    if (difficulty < GIT3_MIN_DIFFICULTY) {
        difficulty = GIT3_MIN_DIFFICULTY;
    }
    
    printf("Mining proof-of-work tag (difficulty: %u bits)...\n", difficulty);
    
    /* Note: For interrupt handling, use pow_optimized.c functions */
    /* TODO: Refactor to use common mining infrastructure */
    
    /* Try different nonces until we find one that meets difficulty */
    while (1) {
        strbuf_reset(&tag_buf);
        
        /* Build tag object */
        strbuf_addf(&tag_buf, "object %s\n", oid_to_hex(object_oid));
        strbuf_addf(&tag_buf, "type %s\n", type);
        strbuf_addf(&tag_buf, "tag %s\n", tag);
        if (tagger)
            strbuf_addf(&tag_buf, "tagger %s\n", tagger);
        
        /* Add tag type for Git3 */
        if (tag_type && strcmp(tag_type, "normal") != 0) {
            strbuf_addf(&tag_buf, "tagtype %s\n", tag_type);
        }
        
        /* Add nonce to message */
        strbuf_addf(&tag_buf, "\n%s\n\nPoW-Nonce: %lu", message, nonce);
        
        /* Calculate hash */
        hash_object_file(the_hash_algo, tag_buf.buf, tag_buf.len, OBJ_TAG, &tag_oid);
        oid_to_hex_r(hex, &tag_oid);
        
        /* Check if it meets difficulty */
        if (check_pow_difficulty(hex, difficulty)) {
            printf("Found PoW tag: %s (nonce=%lu)\n", hex, nonce);
            oidcpy(result_oid, &tag_oid);
            
            if (pow_out) {
                pow_out->nonce = nonce;
                pow_out->difficulty = difficulty;
                pow_out->cumulative_work = calculate_hash_work(hex);
            }
            
            /* Write the tag object */
            write_object_file(tag_buf.buf, tag_buf.len, OBJ_TAG, result_oid);
            
            strbuf_release(&tag_buf);
            return 0;
        }
        
        /* Show progress */
        if (nonce % 100000 == 0 && nonce > 0) {
            printf("  Mining... (nonce: %lu, hash: %s)\n", nonce, hex);
        }
        
        nonce++;
    }
}