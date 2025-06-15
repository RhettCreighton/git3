#include "git-compat-util.h"
#include "hash.h"
#include "hex.h"

/* SHA3-256 hashes of empty tree and blob */
static const struct object_id empty_tree_oid_sha3 = {
	.hash = {
		/* SHA3-256 hash of 'tree 0\0' */
		0x30, 0x21, 0x1e, 0xd4, 0x85, 0xc9, 0x12, 0xe5, 0xbc, 0x28,
		0x5b, 0xd0, 0xbd, 0x89, 0x59, 0xdd, 0xbf, 0xb5, 0x87, 0x5c,
		0xaf, 0xb0, 0xae, 0x28, 0xe0, 0xab, 0xfa, 0x10, 0x77, 0xb2,
		0xb2, 0x14
	},
	.algo = GIT_HASH_SHA3,
};
static const struct object_id empty_blob_oid_sha3 = {
	.hash = {
		/* SHA3-256 hash of empty string */
		0xa7, 0xff, 0xc6, 0xf8, 0xbf, 0x1e, 0xd7, 0x66, 0x51, 0xc1,
		0x47, 0x56, 0xa0, 0x61, 0xd6, 0x62, 0xf5, 0x80, 0xff, 0x4d,
		0xe4, 0x3b, 0x49, 0xfa, 0x82, 0xd8, 0x0a, 0x4b, 0x80, 0xf8,
		0x43, 0x4a
	},
	.algo = GIT_HASH_SHA3,
};
static const struct object_id null_oid_sha3 = {
	.hash = {0},
	.algo = GIT_HASH_SHA3,
};

static void git_hash_sha3_init(struct git_hash_ctx *ctx)
{
	ctx->algop = &hash_algos[GIT_HASH_SHA3];
	git_SHA3_Init(&ctx->state.sha3);
}

static void git_hash_sha3_clone(struct git_hash_ctx *dst, const struct git_hash_ctx *src)
{
	dst->algop = src->algop;
	git_SHA3_Clone(&dst->state.sha3, &src->state.sha3);
}

static void git_hash_sha3_update(struct git_hash_ctx *ctx, const void *data, size_t len)
{
	git_SHA3_Update(&ctx->state.sha3, data, len);
}

static void git_hash_sha3_final(unsigned char *hash, struct git_hash_ctx *ctx)
{
	git_SHA3_Final(hash, &ctx->state.sha3);
}

static void git_hash_sha3_final_oid(struct object_id *oid, struct git_hash_ctx *ctx)
{
	git_SHA3_Final(oid->hash, &ctx->state.sha3);
	/*
	 * This currently does nothing, so the compiler should optimize it out,
	 * but keep it in case we extend the hash size again.
	 */
	memset(oid->hash + GIT_SHA3_RAWSZ, 0, GIT_MAX_RAWSZ - GIT_SHA3_RAWSZ);
	oid->algo = GIT_HASH_SHA3;
}

static void git_hash_unknown_init(struct git_hash_ctx *ctx UNUSED)
{
	BUG("trying to init unknown hash");
}

static void git_hash_unknown_clone(struct git_hash_ctx *dst UNUSED,
				   const struct git_hash_ctx *src UNUSED)
{
	BUG("trying to clone unknown hash");
}

static void git_hash_unknown_update(struct git_hash_ctx *ctx UNUSED,
				    const void *data UNUSED,
				    size_t len UNUSED)
{
	BUG("trying to update unknown hash");
}

static void git_hash_unknown_final(unsigned char *hash UNUSED,
				   struct git_hash_ctx *ctx UNUSED)
{
	BUG("trying to finalize unknown hash");
}

static void git_hash_unknown_final_oid(struct object_id *oid UNUSED,
				       struct git_hash_ctx *ctx UNUSED)
{
	BUG("trying to finalize unknown hash");
}

const struct git_hash_algo hash_algos[GIT_HASH_NALGOS] = {
	{
		.name = NULL,
		.format_id = 0x00000000,
		.rawsz = 0,
		.hexsz = 0,
		.blksz = 0,
		.init_fn = git_hash_unknown_init,
		.clone_fn = git_hash_unknown_clone,
		.update_fn = git_hash_unknown_update,
		.final_fn = git_hash_unknown_final,
		.final_oid_fn = git_hash_unknown_final_oid,
		.empty_tree = NULL,
		.empty_blob = NULL,
		.null_oid = NULL,
	},
	{
		.name = "sha3",
		.format_id = GIT_SHA3_FORMAT_ID,
		.rawsz = GIT_SHA3_RAWSZ,
		.hexsz = GIT_SHA3_HEXSZ,
		.blksz = GIT_SHA3_BLKSZ,
		.init_fn = git_hash_sha3_init,
		.clone_fn = git_hash_sha3_clone,
		.update_fn = git_hash_sha3_update,
		.final_fn = git_hash_sha3_final,
		.final_oid_fn = git_hash_sha3_final_oid,
		.empty_tree = &empty_tree_oid_sha3,
		.empty_blob = &empty_blob_oid_sha3,
		.null_oid = &null_oid_sha3,
	}
};

const struct object_id *null_oid(const struct git_hash_algo *algop)
{
	return algop->null_oid;
}

const char *empty_tree_oid_hex(const struct git_hash_algo *algop)
{
	static char buf[GIT_MAX_HEXSZ + 1];
	return oid_to_hex_r(buf, algop->empty_tree);
}

int hash_algo_by_name(const char *name)
{
	if (!name)
		return GIT_HASH_UNKNOWN;
	for (size_t i = 1; i < GIT_HASH_NALGOS; i++)
		if (!strcmp(name, hash_algos[i].name))
			return i;
	return GIT_HASH_UNKNOWN;
}

int hash_algo_by_id(uint32_t format_id)
{
	for (size_t i = 1; i < GIT_HASH_NALGOS; i++)
		if (format_id == hash_algos[i].format_id)
			return i;
	return GIT_HASH_UNKNOWN;
}

int hash_algo_by_length(size_t len)
{
	for (size_t i = 1; i < GIT_HASH_NALGOS; i++)
		if (len == hash_algos[i].rawsz)
			return i;
	return GIT_HASH_UNKNOWN;
}

const struct git_hash_algo *unsafe_hash_algo(const struct git_hash_algo *algop)
{
	/* If we have a faster "unsafe" implementation, use that. */
	if (algop->unsafe)
		return algop->unsafe;
	/* Otherwise use the default one. */
	return algop;
}
