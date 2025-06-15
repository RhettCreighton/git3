#ifndef HASH_H
#define HASH_H

/* SHA3-256 is the only supported hash algorithm */
#define SHA3_BACKEND "SHA3_BLK"
#include "sha3/block/sha3.h"

#ifndef platform_SHA3_CTX
#define platform_SHA3_CTX	blk_SHA3_CTX
#define platform_SHA3_Init	blk_SHA3_Init
#define platform_SHA3_Update	blk_SHA3_Update
#define platform_SHA3_Final	blk_SHA3_Final
#endif

#define git_SHA3_CTX		platform_SHA3_CTX
#define git_SHA3_Init		platform_SHA3_Init
#define git_SHA3_Update		platform_SHA3_Update
#define git_SHA3_Final		platform_SHA3_Final

#ifdef platform_SHA3_Clone
#define git_SHA3_Clone	platform_SHA3_Clone
#endif

#ifndef SHA3_NEEDS_CLONE_HELPER
static inline void git_SHA3_Clone(git_SHA3_CTX *dst, const git_SHA3_CTX *src)
{
	memcpy(dst, src, sizeof(*dst));
}
#endif

/*
 * Note that these constants are suitable for indexing the hash_algos array and
 * comparing against each other, but are otherwise arbitrary, so they should not
 * be exposed to the user or serialized to disk.  To know whether a
 * git_hash_algo struct points to some usable hash function, test the format_id
 * field for being non-zero.  Use the name field for user-visible situations and
 * the format_id field for fixed-length fields on disk.
 */
/* An unknown hash function. */
#define GIT_HASH_UNKNOWN 0
/* SHA3-256 */
#define GIT_HASH_SHA3 1
/* Number of algorithms supported (including unknown). */
#define GIT_HASH_NALGOS (GIT_HASH_SHA3 + 1)

/* "sha3", big-endian */
#define GIT_SHA3_FORMAT_ID 0x73686133

/* The length in bytes and in hex digits of an object name (SHA3-256 value). */
#define GIT_SHA3_RAWSZ 32
#define GIT_SHA3_HEXSZ (2 * GIT_SHA3_RAWSZ)
/* The block size of SHA3-256. */
#define GIT_SHA3_BLKSZ 136

/* The length in byte and in hex digits of the largest possible hash value. */
#define GIT_MAX_RAWSZ GIT_SHA3_RAWSZ
#define GIT_MAX_HEXSZ GIT_SHA3_HEXSZ
/* The largest possible block size for any supported hash. */
#define GIT_MAX_BLKSZ GIT_SHA3_BLKSZ

struct object_id {
	unsigned char hash[GIT_MAX_RAWSZ];
	int algo;	/* XXX requires 4-byte alignment */
};

#define GET_OID_QUIETLY                  01
#define GET_OID_COMMIT                   02
#define GET_OID_COMMITTISH               04
#define GET_OID_TREE                    010
#define GET_OID_TREEISH                 020
#define GET_OID_BLOB                    040
#define GET_OID_FOLLOW_SYMLINKS        0100
#define GET_OID_RECORD_PATH            0200
#define GET_OID_ONLY_TO_DIE           04000
#define GET_OID_REQUIRE_PATH         010000
#define GET_OID_HASH_ANY             020000
#define GET_OID_SKIP_AMBIGUITY_CHECK 040000

#define GET_OID_DISAMBIGUATORS \
	(GET_OID_COMMIT | GET_OID_COMMITTISH | \
	GET_OID_TREE | GET_OID_TREEISH | \
	GET_OID_BLOB)

enum get_oid_result {
	FOUND = 0,
	MISSING_OBJECT = -1, /* The requested object is missing */
	SHORT_NAME_AMBIGUOUS = -2,
	/* The following only apply when symlinks are followed */
	DANGLING_SYMLINK = -4, /*
				* The initial symlink is there, but
				* (transitively) points to a missing
				* in-tree file
				*/
	SYMLINK_LOOP = -5,
	NOT_DIR = -6, /*
		       * Somewhere along the symlink chain, a path is
		       * requested which contains a file as a
		       * non-final element.
		       */
};

#ifdef USE_THE_REPOSITORY_VARIABLE
# include "repository.h"
# define the_hash_algo the_repository->hash_algo
#endif

/* A suitably aligned type for stack allocations of hash contexts. */
struct git_hash_ctx {
	const struct git_hash_algo *algop;
	union {
		git_SHA3_CTX sha3;
	} state;
};

typedef void (*git_hash_init_fn)(struct git_hash_ctx *ctx);
typedef void (*git_hash_clone_fn)(struct git_hash_ctx *dst, const struct git_hash_ctx *src);
typedef void (*git_hash_update_fn)(struct git_hash_ctx *ctx, const void *in, size_t len);
typedef void (*git_hash_final_fn)(unsigned char *hash, struct git_hash_ctx *ctx);
typedef void (*git_hash_final_oid_fn)(struct object_id *oid, struct git_hash_ctx *ctx);

struct git_hash_algo {
	/*
	 * The name of the algorithm, as appears in the config file and in
	 * messages.
	 */
	const char *name;

	/* A four-byte version identifier, used in pack indices. */
	uint32_t format_id;

	/* The length of the hash in binary. */
	size_t rawsz;

	/* The length of the hash in hex characters. */
	size_t hexsz;

	/* The block size of the hash. */
	size_t blksz;

	/* The hash initialization function. */
	git_hash_init_fn init_fn;

	/* The hash context cloning function. */
	git_hash_clone_fn clone_fn;

	/* The hash update function. */
	git_hash_update_fn update_fn;

	/* The hash finalization function. */
	git_hash_final_fn final_fn;

	/* The hash finalization function for object IDs. */
	git_hash_final_oid_fn final_oid_fn;

	/* The OID of the empty tree. */
	const struct object_id *empty_tree;

	/* The OID of the empty blob. */
	const struct object_id *empty_blob;

	/* The all-zeros OID. */
	const struct object_id *null_oid;

	/* The unsafe variant of this hash function, if one exists. */
	const struct git_hash_algo *unsafe;
};
extern const struct git_hash_algo hash_algos[GIT_HASH_NALGOS];

static inline void git_hash_clone(struct git_hash_ctx *dst, const struct git_hash_ctx *src)
{
	src->algop->clone_fn(dst, src);
}

static inline void git_hash_update(struct git_hash_ctx *ctx, const void *in, size_t len)
{
	ctx->algop->update_fn(ctx, in, len);
}

static inline void git_hash_final(unsigned char *hash, struct git_hash_ctx *ctx)
{
	ctx->algop->final_fn(hash, ctx);
}

static inline void git_hash_final_oid(struct object_id *oid, struct git_hash_ctx *ctx)
{
	ctx->algop->final_oid_fn(oid, ctx);
}

/*
 * Return a GIT_HASH_* constant based on the name.  Returns GIT_HASH_UNKNOWN if
 * the name doesn't match a known algorithm.
 */
int hash_algo_by_name(const char *name);
/* Identical, except based on the format ID. */
int hash_algo_by_id(uint32_t format_id);
/* Identical, except based on the length. */
int hash_algo_by_length(size_t len);
/* Identical, except for a pointer to struct git_hash_algo. */
static inline int hash_algo_by_ptr(const struct git_hash_algo *p)
{
	size_t i;
	for (i = 0; i < GIT_HASH_NALGOS; i++) {
		const struct git_hash_algo *algop = &hash_algos[i];
		if (p == algop)
			return i;
	}
	return GIT_HASH_UNKNOWN;
}

const struct git_hash_algo *unsafe_hash_algo(const struct git_hash_algo *algop);

const struct object_id *null_oid(const struct git_hash_algo *algop);

static inline int hashcmp(const unsigned char *sha1, const unsigned char *sha2, const struct git_hash_algo *algop)
{
	return memcmp(sha1, sha2, algop->rawsz);
}

static inline int hasheq(const unsigned char *sha1, const unsigned char *sha2, const struct git_hash_algo *algop)
{
	return !memcmp(sha1, sha2, algop->rawsz);
}

static inline void hashcpy(unsigned char *sha_dst, const unsigned char *sha_src,
			   const struct git_hash_algo *algop)
{
	memcpy(sha_dst, sha_src, algop->rawsz);
}

static inline void hashclr(unsigned char *hash, const struct git_hash_algo *algop)
{
	memset(hash, 0, algop->rawsz);
}

static inline int oidcmp(const struct object_id *oid1, const struct object_id *oid2)
{
	return memcmp(oid1->hash, oid2->hash, GIT_MAX_RAWSZ);
}

static inline int oideq(const struct object_id *oid1, const struct object_id *oid2)
{
	return !memcmp(oid1->hash, oid2->hash, GIT_MAX_RAWSZ);
}

static inline void oidcpy(struct object_id *dst, const struct object_id *src)
{
	memcpy(dst->hash, src->hash, GIT_MAX_RAWSZ);
	dst->algo = src->algo;
}

static inline void oidread(struct object_id *oid, const unsigned char *hash,
			   const struct git_hash_algo *algop)
{
	memcpy(oid->hash, hash, algop->rawsz);
	if (algop->rawsz < GIT_MAX_RAWSZ)
		memset(oid->hash + algop->rawsz, 0, GIT_MAX_RAWSZ - algop->rawsz);
	oid->algo = hash_algo_by_ptr(algop);
}

static inline void oidclr(struct object_id *oid,
			  const struct git_hash_algo *algop)
{
	memset(oid->hash, 0, GIT_MAX_RAWSZ);
	oid->algo = hash_algo_by_ptr(algop);
}

static inline struct object_id *oiddup(const struct object_id *src)
{
	struct object_id *dst = xmalloc(sizeof(struct object_id));
	oidcpy(dst, src);
	return dst;
}

static inline void oid_set_algo(struct object_id *oid, const struct git_hash_algo *algop)
{
	oid->algo = hash_algo_by_ptr(algop);
}

/*
 * Converts a cryptographic hash (e.g. SHA-1) into an int-sized hash code
 * for use in hash tables. Cryptographic hashes are supposed to have
 * uniform distribution, so in contrast to `memhash()`, this just copies
 * the first `sizeof(int)` bytes without shuffling any bits. Note that
 * the results will be different on big-endian and little-endian
 * platforms, so they should not be stored or transferred over the net.
 */
static inline unsigned int oidhash(const struct object_id *oid)
{
	/*
	 * Equivalent to 'return *(unsigned int *)oid->hash;', but safe on
	 * platforms that don't support unaligned reads.
	 */
	unsigned int hash;
	memcpy(&hash, oid->hash, sizeof(hash));
	return hash;
}

static inline int is_null_oid(const struct object_id *oid)
{
	static const unsigned char null_hash[GIT_MAX_RAWSZ];
	return !memcmp(oid->hash, null_hash, GIT_MAX_RAWSZ);
}

const char *empty_tree_oid_hex(const struct git_hash_algo *algop);

static inline int is_empty_blob_oid(const struct object_id *oid,
				    const struct git_hash_algo *algop)
{
	return oideq(oid, algop->empty_blob);
}

static inline int is_empty_tree_oid(const struct object_id *oid,
				    const struct git_hash_algo *algop)
{
	return oideq(oid, algop->empty_tree);
}

#endif
