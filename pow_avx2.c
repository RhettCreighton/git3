/*
 * AVX2-optimized proof-of-work mining for Git3
 * 
 * This implementation uses AVX2 to compute multiple SHA3 hashes
 * in parallel for faster mining performance.
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "pow.h"
#include "hash.h"
#include "hex.h"
#include "strbuf.h"
#include "sha3_avx2.h"
#include "object-file.h"
#include "object.h"
#include <immintrin.h>
#include <signal.h>
#include <pthread.h>

#ifdef __AVX2__

/* Number of parallel hashes to compute */
#define PARALLEL_HASHES 4

/* Mining thread data */
typedef struct {
    const char *base_data;
    size_t base_len;
    size_t nonce_offset;
    uint64_t start_nonce;
    uint64_t end_nonce;
    uint32_t difficulty;
    volatile int *found;
    uint64_t *result_nonce;
    unsigned char *result_hash;
    pthread_mutex_t *result_mutex;
} mining_thread_data;

/* Global interrupt flag */
static volatile sig_atomic_t mining_interrupted = 0;

/* Interrupt handler */
static void handle_interrupt(int sig)
{
    mining_interrupted = 1;
    printf("\n\nAVX2 mining interrupted by user (Ctrl+C)...\n");
}

/* Check if hash meets difficulty using AVX2 */
static int check_difficulty_avx2(const unsigned char *hash, uint32_t difficulty)
{
    uint32_t zero_bits = 0;
    
    for (size_t i = 0; i < 32 && zero_bits < difficulty; i++) {
        unsigned char byte = hash[i];
        if (byte == 0) {
            zero_bits += 8;
        } else {
            /* Count leading zeros in byte */
            zero_bits += __builtin_clz(byte) - 24;
            break;
        }
    }
    
    return zero_bits >= difficulty;
}

/* Mining worker thread */
static void *mining_worker_avx2(void *arg)
{
    mining_thread_data *data = (mining_thread_data *)arg;
    char nonce_buf[32];
    unsigned char hash[32];
    
    for (uint64_t nonce = data->start_nonce; 
         nonce < data->end_nonce && !*data->found && !mining_interrupted; 
         nonce++) {
        
        /* Build commit with nonce */
        struct strbuf commit_buf = STRBUF_INIT;
        strbuf_add(&commit_buf, data->base_data, data->nonce_offset);
        snprintf(nonce_buf, sizeof(nonce_buf), "%lu", nonce);
        strbuf_addstr(&commit_buf, nonce_buf);
        strbuf_add(&commit_buf, 
                   data->base_data + data->nonce_offset + strlen("NONCE_PLACEHOLDER"),
                   data->base_len - data->nonce_offset - strlen("NONCE_PLACEHOLDER"));
        
        /* Compute SHA3-256 hash */
        if (sha3_avx2_available()) {
            sha3_256_avx2((unsigned char *)commit_buf.buf, commit_buf.len, hash);
        } else {
            /* Fallback to regular SHA3 - compute hash directly */
            struct object_id temp_oid;
            hash_object_file(the_hash_algo, commit_buf.buf, commit_buf.len, 
                            OBJ_COMMIT, &temp_oid);
            memcpy(hash, temp_oid.hash, 32);
        }
        
        /* Check difficulty */
        if (check_difficulty_avx2(hash, data->difficulty)) {
            pthread_mutex_lock(data->result_mutex);
            if (!*data->found) {
                *data->found = 1;
                *data->result_nonce = nonce;
                memcpy(data->result_hash, hash, 32);
            }
            pthread_mutex_unlock(data->result_mutex);
            strbuf_release(&commit_buf);
            return NULL;
        }
        
        strbuf_release(&commit_buf);
        
        /* Progress reporting */
        if (nonce % 100000 == 0) {
            char hex[65];
            for (int i = 0; i < 32; i++) {
                snprintf(hex + i * 2, 3, "%02x", hash[i]);
            }
            hex[64] = '\0';
            printf("  AVX2 mining... (nonce: %lu, hash: %s)\n", nonce, hex);
        }
    }
    
    return NULL;
}

/* Multi-threaded AVX2 mining */
int mine_pow_commit_avx2(const struct object_id *tree_oid,
                        const struct object_id *parent_oid,
                        const char *author,
                        const char *committer,
                        const char *message,
                        enum commit_type type,
                        uint32_t difficulty,
                        struct object_id *result_oid,
                        struct pow_data *pow_out)
{
    if (!sha3_avx2_available()) {
        /* Fall back to regular mining */
        return mine_pow_commit(tree_oid, parent_oid, author, committer,
                              message, type, difficulty, result_oid, pow_out);
    }
    
    printf("Mining with AVX2 optimization (difficulty: %u bits)...\n", difficulty);
    
    /* Set up interrupt handler */
    struct sigaction sa;
    sa.sa_handler = handle_interrupt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    /* Build base commit data */
    struct strbuf base_buf = STRBUF_INIT;
    
    /* Add fixed parts */
    strbuf_addf(&base_buf, "tree %s\n", oid_to_hex(tree_oid));
    if (parent_oid) {
        strbuf_addf(&base_buf, "parent %s\n", oid_to_hex(parent_oid));
    }
    strbuf_addf(&base_buf, "author %s\n", author);
    strbuf_addf(&base_buf, "committer %s\n", committer);
    strbuf_addch(&base_buf, '\n');
    
    /* Add message and PoW prefix */
    if (type == COMMIT_TYPE_FREEZE) {
        strbuf_addstr(&base_buf, "[FREEZE] ");
    } else if (type == COMMIT_TYPE_CLEAN) {
        strbuf_addstr(&base_buf, "[CLEAN] ");
    }
    strbuf_addstr(&base_buf, message);
    strbuf_addstr(&base_buf, "\n\nPoW-Nonce: ");
    
    size_t nonce_offset = base_buf.len;
    strbuf_addstr(&base_buf, "NONCE_PLACEHOLDER");
    
    /* Add remaining fields */
    uint64_t parent_work = parent_oid ? calculate_total_work(parent_oid) : 0;
    strbuf_addf(&base_buf, "\nPoW-Difficulty: %u\n", difficulty);
    strbuf_addf(&base_buf, "PoW-Parent-Work: %lu", parent_work);
    
    /* Set up multi-threading */
    int num_threads = 4; /* Use 4 threads for AVX2 mining */
    pthread_t threads[4];
    mining_thread_data thread_data[4];
    
    volatile int found = 0;
    uint64_t result_nonce = 0;
    unsigned char result_hash[32];
    pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    uint64_t nonce_range = UINT64_MAX / num_threads;
    
    /* Start mining threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].base_data = base_buf.buf;
        thread_data[i].base_len = base_buf.len;
        thread_data[i].nonce_offset = nonce_offset;
        thread_data[i].start_nonce = i * nonce_range;
        thread_data[i].end_nonce = (i + 1) * nonce_range;
        thread_data[i].difficulty = difficulty;
        thread_data[i].found = &found;
        thread_data[i].result_nonce = &result_nonce;
        thread_data[i].result_hash = result_hash;
        thread_data[i].result_mutex = &result_mutex;
        
        pthread_create(&threads[i], NULL, mining_worker_avx2, &thread_data[i]);
    }
    
    /* Wait for threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    if (found && !mining_interrupted) {
        /* Build final commit with found nonce */
        struct strbuf final_buf = STRBUF_INIT;
        strbuf_add(&final_buf, base_buf.buf, nonce_offset);
        strbuf_addf(&final_buf, "%lu", result_nonce);
        strbuf_add(&final_buf, 
                   base_buf.buf + nonce_offset + strlen("NONCE_PLACEHOLDER"),
                   base_buf.len - nonce_offset - strlen("NONCE_PLACEHOLDER"));
        
        /* Convert hash to hex and display */
        char hex[65];
        for (int i = 0; i < 32; i++) {
            snprintf(hex + i * 2, 3, "%02x", result_hash[i]);
        }
        hex[64] = '\0';
        
        uint64_t work = calculate_hash_work(hex);
        uint64_t total_work = parent_work + work;
        
        printf("\n✓ Found valid PoW hash with AVX2: %s\n", hex);
        printf("  Nonce: %lu\n", result_nonce);
        printf("  Work: %lu (2^%u)\n", work, difficulty);
        printf("  Total work: %lu\n", total_work);
        
        /* Write object */
        if (write_object_file(final_buf.buf, final_buf.len, OBJ_COMMIT, result_oid) < 0) {
            error("Failed to write commit object");
            strbuf_release(&final_buf);
            strbuf_release(&base_buf);
            return -1;
        }
        
        if (pow_out) {
            pow_out->nonce = result_nonce;
            pow_out->difficulty = difficulty;
            pow_out->work = work;
            pow_out->cumulative_work = total_work;
        }
        
        strbuf_release(&final_buf);
        strbuf_release(&base_buf);
        return 0;
    }
    
    strbuf_release(&base_buf);
    return -1;
}

#else /* !__AVX2__ */

/* Fallback when AVX2 not available */
int mine_pow_commit_avx2(const struct object_id *tree_oid,
                        const struct object_id *parent_oid,
                        const char *author,
                        const char *committer,
                        const char *message,
                        enum commit_type type,
                        uint32_t difficulty,
                        struct object_id *result_oid,
                        struct pow_data *pow_out)
{
    /* Fall back to regular mining */
    return mine_pow_commit(tree_oid, parent_oid, author, committer,
                          message, type, difficulty, result_oid, pow_out);
}

#endif /* __AVX2__ */