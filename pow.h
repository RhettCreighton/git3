#ifndef POW_H
#define POW_H

#include <stdint.h>

struct object_id;

/* Minimum work requirement (1M = 2^20) */
#define GIT3_MIN_WORK 1048576
#define GIT3_MIN_DIFFICULTY 20

/* Proof of Work data structure */
struct pow_data {
    uint64_t nonce;
    uint32_t difficulty;
    uint64_t work;           /* Work for this object (2^difficulty) */
    uint64_t cumulative_work; /* Total work including parents */
};

/* Commit types for Git3 */
enum commit_type {
    COMMIT_TYPE_NORMAL = 0,
    COMMIT_TYPE_FREEZE,
    COMMIT_TYPE_CLEAN
};

/* Calculate work based on leading zeros in hash */
uint64_t calculate_hash_work(const char *hash_hex);

/* Calculate total cumulative work for a commit */
uint64_t calculate_total_work(const struct object_id *commit_oid);

/* Format work as human-readable string */
void format_work(uint64_t work, char *buffer, size_t size);

/* Mine a commit with proof-of-work */
int mine_pow_commit(const struct object_id *tree_oid,
                    const struct object_id *parent_oid,
                    const char *author,
                    const char *committer,
                    const char *message,
                    enum commit_type type,
                    uint32_t difficulty,
                    struct object_id *result_oid,
                    struct pow_data *pow_out);

/* Check if hash meets difficulty requirement */
int check_pow_difficulty(const char *hash_hex, uint32_t difficulty);

/* Get the appropriate PoW difficulty for the current branch */
int get_pow_difficulty_for_branch(void);

/* Set difficulty for a branch pattern in config */
int set_pow_difficulty_config(const char *pattern, int difficulty);

/* Mine a tag with proof-of-work */
int mine_pow_tag(const struct object_id *object_oid,
                 const char *type,
                 const char *tag,
                 const char *tagger,
                 const char *message,
                 const char *tag_type,
                 uint32_t difficulty,
                 struct object_id *result_oid,
                 struct pow_data *pow_out);

/* Optimized mining function (uses interrupt handling) */
int mine_pow_commit_optimized(const struct object_id *tree_oid,
                             const struct object_id *parent_oid,
                             const char *author,
                             const char *committer,
                             const char *message,
                             enum commit_type type,
                             uint32_t difficulty,
                             struct object_id *result_oid,
                             struct pow_data *pow_out);

#endif /* POW_H */