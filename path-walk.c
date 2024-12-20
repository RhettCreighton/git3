/*
 * path-walk.c: implementation for path-based walks of the object graph.
 */
#include "git-compat-util.h"
#include "path-walk.h"
#include "blob.h"
#include "commit.h"
#include "dir.h"
#include "hashmap.h"
#include "hex.h"
#include "object.h"
#include "oid-array.h"
#include "revision.h"
#include "string-list.h"
#include "strmap.h"
#include "trace2.h"
#include "tree.h"
#include "tree-walk.h"

struct type_and_oid_list {
	enum object_type type;
	struct oid_array oids;
};

#define TYPE_AND_OID_LIST_INIT { \
	.type = OBJ_NONE, 	 \
	.oids = OID_ARRAY_INIT	 \
}

struct path_walk_context {
	/**
	 * Repeats of data in 'struct path_walk_info' for
	 * access with fewer characters.
	 */
	struct repository *repo;
	struct rev_info *revs;
	struct path_walk_info *info;

	/**
	 * Map a path to a 'struct type_and_oid_list'
	 * containing the objects discovered at that
	 * path.
	 */
	struct strmap paths_to_lists;

	/**
	 * Store the current list of paths in a stack, to
	 * facilitate depth-first-search without recursion.
	 *
	 * Use path_stack_pushed to indicate whether a path
	 * was previously added to path_stack.
	 */
	struct string_list path_stack;
	struct strset path_stack_pushed;
};

static void push_to_stack(struct path_walk_context *ctx,
			  const char *path)
{
	if (strset_contains(&ctx->path_stack_pushed, path))
		return;

	strset_add(&ctx->path_stack_pushed, path);
	string_list_append(&ctx->path_stack, path);
}

static int add_tree_entries(struct path_walk_context *ctx,
			    const char *base_path,
			    struct object_id *oid)
{
	struct tree_desc desc;
	struct name_entry entry;
	struct strbuf path = STRBUF_INIT;
	size_t base_len;
	struct tree *tree = lookup_tree(ctx->repo, oid);

	if (!tree) {
		error(_("failed to walk children of tree %s: not found"),
		      oid_to_hex(oid));
		return -1;
	} else if (parse_tree_gently(tree, 1)) {
		error("bad tree object %s", oid_to_hex(oid));
		return -1;
	}

	strbuf_addstr(&path, base_path);
	base_len = path.len;

	parse_tree(tree);
	init_tree_desc(&desc, &tree->object.oid, tree->buffer, tree->size);
	while (tree_entry(&desc, &entry)) {
		struct type_and_oid_list *list;
		struct object *o;
		/* Not actually true, but we will ignore submodules later. */
		enum object_type type = S_ISDIR(entry.mode) ? OBJ_TREE : OBJ_BLOB;

		/* Skip submodules. */
		if (S_ISGITLINK(entry.mode))
			continue;

		if (type == OBJ_TREE) {
			struct tree *child = lookup_tree(ctx->repo, &entry.oid);
			o = child ? &child->object : NULL;
		} else if (type == OBJ_BLOB) {
			struct blob *child = lookup_blob(ctx->repo, &entry.oid);
			o = child ? &child->object : NULL;
		} else {
			BUG("invalid type for tree entry: %d", type);
		}

		if (!o) {
			error(_("failed to find object %s"),
			      oid_to_hex(&o->oid));
			return -1;
		}

		/* Skip this object if already seen. */
		if (o->flags & SEEN)
			continue;
		o->flags |= SEEN;

		strbuf_setlen(&path, base_len);
		strbuf_add(&path, entry.path, entry.pathlen);

		/*
		 * Trees will end with "/" for concatenation and distinction
		 * from blobs at the same path.
		 */
		if (type == OBJ_TREE)
			strbuf_addch(&path, '/');

		if (!(list = strmap_get(&ctx->paths_to_lists, path.buf))) {
			CALLOC_ARRAY(list, 1);
			list->type = type;
			strmap_put(&ctx->paths_to_lists, path.buf, list);
		}
		push_to_stack(ctx, path.buf);
		oid_array_append(&list->oids, &entry.oid);
	}

	free_tree_buffer(tree);
	strbuf_release(&path);
	return 0;
}

/*
 * For each path in paths_to_explore, walk the trees another level
 * and add any found blobs to the batch (but only if they exist and
 * haven't been added yet).
 */
static int walk_path(struct path_walk_context *ctx,
		     const char *path)
{
	struct type_and_oid_list *list;
	int ret = 0;

	list = strmap_get(&ctx->paths_to_lists, path);

	if (!list->oids.nr)
		return 0;

	/* Evaluate function pointer on this data. */
	ret = ctx->info->path_fn(path, &list->oids, list->type,
				 ctx->info->path_fn_data);

	/* Expand data for children. */
	if (list->type == OBJ_TREE) {
		for (size_t i = 0; i < list->oids.nr; i++) {
			ret |= add_tree_entries(ctx,
					    path,
					    &list->oids.oid[i]);
		}
	}

	oid_array_clear(&list->oids);
	strmap_remove(&ctx->paths_to_lists, path, 1);
	return ret;
}

static void clear_paths_to_lists(struct strmap *map)
{
	struct hashmap_iter iter;
	struct strmap_entry *e;

	hashmap_for_each_entry(&map->map, &iter, e, ent) {
		struct type_and_oid_list *list = e->value;
		oid_array_clear(&list->oids);
	}
	strmap_clear(map, 1);
	strmap_init(map);
}

/**
 * Given the configuration of 'info', walk the commits based on 'info->revs' and
 * call 'info->path_fn' on each discovered path.
 *
 * Returns nonzero on an error.
 */
int walk_objects_by_path(struct path_walk_info *info)
{
	const char *root_path = "";
	int ret = 0;
	size_t commits_nr = 0, paths_nr = 0;
	struct commit *c;
	struct type_and_oid_list *root_tree_list;
	struct path_walk_context ctx = {
		.repo = info->revs->repo,
		.revs = info->revs,
		.info = info,
		.path_stack = STRING_LIST_INIT_DUP,
		.path_stack_pushed = STRSET_INIT,
		.paths_to_lists = STRMAP_INIT
	};

	trace2_region_enter("path-walk", "commit-walk", info->revs->repo);

	/* Insert a single list for the root tree into the paths. */
	CALLOC_ARRAY(root_tree_list, 1);
	root_tree_list->type = OBJ_TREE;
	strmap_put(&ctx.paths_to_lists, root_path, root_tree_list);
	push_to_stack(&ctx, root_path);

	if (prepare_revision_walk(info->revs))
		die(_("failed to setup revision walk"));

	while ((c = get_revision(info->revs))) {
		struct object_id *oid = get_commit_tree_oid(c);
		struct tree *t;
		commits_nr++;

		oid = get_commit_tree_oid(c);
		t = lookup_tree(info->revs->repo, oid);

		if (!t) {
			error("could not find tree %s", oid_to_hex(oid));
			return -1;
		}

		if (t->object.flags & SEEN)
			continue;
		t->object.flags |= SEEN;
		oid_array_append(&root_tree_list->oids, oid);
	}

	trace2_data_intmax("path-walk", ctx.repo, "commits", commits_nr);
	trace2_region_leave("path-walk", "commit-walk", info->revs->repo);

	trace2_region_enter("path-walk", "path-walk", info->revs->repo);
	while (!ret && ctx.path_stack.nr) {
		char *path = ctx.path_stack.items[ctx.path_stack.nr - 1].string;
		ctx.path_stack.nr--;
		paths_nr++;

		ret = walk_path(&ctx, path);

		free(path);
	}
	trace2_data_intmax("path-walk", ctx.repo, "paths", paths_nr);
	trace2_region_leave("path-walk", "path-walk", info->revs->repo);

	clear_paths_to_lists(&ctx.paths_to_lists);
	strset_clear(&ctx.path_stack_pushed);
	string_list_clear(&ctx.path_stack, 0);
	return ret;
}

void path_walk_info_init(struct path_walk_info *info)
{
	struct path_walk_info empty = PATH_WALK_INFO_INIT;
	memcpy(info, &empty, sizeof(empty));
}

void path_walk_info_clear(struct path_walk_info *info UNUSED)
{
	/*
	 * This destructor is empty for now, as info->revs
	 * is not owned by 'struct path_walk_info'.
	 */
}
