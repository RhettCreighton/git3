/*
 * builtin/pow-config.c - Configure proof-of-work difficulty for Git3
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "builtin.h"
#include "config.h"
#include "gettext.h"
#include "parse-options.h"
#include "pow.h"
#include "refs.h"
#include "repository.h"
#include "strbuf.h"

static const char * const builtin_pow_config_usage[] = {
	N_("git3 pow-config [--list]"),
	N_("git3 pow-config <branch> <difficulty>"),
	N_("git3 pow-config --unset <branch>"),
	N_("git3 pow-config --default <difficulty>"),
	NULL
};

static int list_pow_config(void)
{
	struct strbuf key = STRBUF_INIT;
	int ret = 0;
	
	printf("Git3 Proof-of-Work Configuration\n");
	printf("=================================\n\n");
	
	/* Show default patterns */
	printf("Default patterns:\n");
	printf("  dev/*      : 8 bits\n");
	printf("  feature/*  : 10 bits\n");
	printf("  main       : 12 bits\n");
	printf("  master     : 12 bits\n");
	printf("  release/*  : 16 bits\n");
	printf("  default    : 20 bits\n\n");
	
	printf("Branch-specific configuration:\n");
	
	/* List all branch.*.powdifficulty configs */
	git_config(git_default_config, NULL);
	
	/* TODO: Iterate through config to show branch-specific settings */
	printf("  (use 'git3 pow-config <branch> <difficulty>' to configure)\n");
	
	strbuf_release(&key);
	return ret;
}

static int set_branch_difficulty(const char *branch, int difficulty)
{
	struct strbuf key = STRBUF_INIT;
	char value[32];
	int ret;
	
	if (difficulty < GIT3_MIN_DIFFICULTY) {
		error(_("difficulty must be at least %d bits"), GIT3_MIN_DIFFICULTY);
		return -1;
	}
	
	if (difficulty > 32) {
		error(_("difficulty cannot exceed 32 bits"));
		return -1;
	}
	
	strbuf_addf(&key, "branch.%s.powdifficulty", branch);
	snprintf(value, sizeof(value), "%d", difficulty);
	
	ret = git_config_set_gently(key.buf, value);
	if (ret < 0) {
		error(_("failed to set configuration"));
	} else {
		printf("Set PoW difficulty for branch '%s' to %d bits\n", branch, difficulty);
	}
	
	strbuf_release(&key);
	return ret;
}

static int unset_branch_difficulty(const char *branch)
{
	struct strbuf key = STRBUF_INIT;
	int ret;
	
	strbuf_addf(&key, "branch.%s.powdifficulty", branch);
	
	ret = git_config_set_gently(key.buf, NULL);
	if (ret < 0) {
		error(_("failed to unset configuration"));
	} else {
		printf("Removed PoW difficulty configuration for branch '%s'\n", branch);
	}
	
	strbuf_release(&key);
	return ret;
}

static int set_default_difficulty(int difficulty)
{
	char value[32];
	int ret;
	
	if (difficulty < GIT3_MIN_DIFFICULTY) {
		error(_("difficulty must be at least %d bits"), GIT3_MIN_DIFFICULTY);
		return -1;
	}
	
	if (difficulty > 32) {
		error(_("difficulty cannot exceed 32 bits"));
		return -1;
	}
	
	snprintf(value, sizeof(value), "%d", difficulty);
	
	ret = git_config_set_gently("pow.difficulty.default", value);
	if (ret < 0) {
		error(_("failed to set default difficulty"));
	} else {
		printf("Set default PoW difficulty to %d bits\n", difficulty);
	}
	
	return ret;
}

int cmd_pow_config(int argc, const char **argv, const char *prefix, struct repository *repo UNUSED)
{
	int list = 0;
	int unset = 0;
	int set_default = 0;
	struct option options[] = {
		OPT_BOOL('l', "list", &list, N_("list all PoW configurations")),
		OPT_BOOL('u', "unset", &unset, N_("unset branch difficulty")),
		OPT_BOOL('d', "default", &set_default, N_("set default difficulty")),
		OPT_END()
	};
	
	argc = parse_options(argc, argv, prefix, options,
			     builtin_pow_config_usage, 0);
	
	if (list) {
		if (argc > 0)
			usage_with_options(builtin_pow_config_usage, options);
		return list_pow_config();
	}
	
	if (set_default) {
		if (argc != 1)
			usage_with_options(builtin_pow_config_usage, options);
		
		int difficulty = atoi(argv[0]);
		return set_default_difficulty(difficulty);
	}
	
	if (unset) {
		if (argc != 1)
			usage_with_options(builtin_pow_config_usage, options);
		
		return unset_branch_difficulty(argv[0]);
	}
	
	/* Default: set branch difficulty */
	if (argc != 2)
		usage_with_options(builtin_pow_config_usage, options);
	
	const char *branch = argv[0];
	int difficulty = atoi(argv[1]);
	
	return set_branch_difficulty(branch, difficulty);
}