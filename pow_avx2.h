#ifndef POW_AVX2_H
#define POW_AVX2_H

#include "pow.h"
#include "object.h"

/* AVX2-optimized proof-of-work mining for commits */
int mine_pow_commit_avx2(const struct object_id *tree_oid,
                        const struct object_id *parent_oid,
                        const char *author,
                        const char *committer,
                        const char *message,
                        enum commit_type type,
                        uint32_t difficulty,
                        struct object_id *result_oid,
                        struct pow_data *pow_out);

#endif /* POW_AVX2_H */