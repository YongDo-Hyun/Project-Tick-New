/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 2026
 *  Project Tick. All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/ioctl.h>
#include <sys/syscall.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "ls.h"
#include "extern.h"

#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif

#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif

#ifndef STATX_TYPE
struct statx_timestamp {
	int64_t tv_sec;
	uint32_t tv_nsec;
	int32_t __reserved;
};

struct statx {
	uint32_t stx_mask;
	uint32_t stx_blksize;
	uint64_t stx_attributes;
	uint32_t stx_nlink;
	uint32_t stx_uid;
	uint32_t stx_gid;
	uint16_t stx_mode;
	uint16_t __spare0[1];
	uint64_t stx_ino;
	uint64_t stx_size;
	uint64_t stx_blocks;
	uint64_t stx_attributes_mask;
	struct statx_timestamp stx_atime;
	struct statx_timestamp stx_btime;
	struct statx_timestamp stx_ctime;
	struct statx_timestamp stx_mtime;
	uint32_t stx_rdev_major;
	uint32_t stx_rdev_minor;
	uint32_t stx_dev_major;
	uint32_t stx_dev_minor;
	uint64_t stx_mnt_id;
	uint32_t stx_dio_mem_align;
	uint32_t stx_dio_offset_align;
	uint64_t __spare3[12];
};

#define STATX_TYPE 0x00000001U
#define STATX_MODE 0x00000002U
#define STATX_NLINK 0x00000004U
#define STATX_UID 0x00000008U
#define STATX_GID 0x00000010U
#define STATX_ATIME 0x00000020U
#define STATX_MTIME 0x00000040U
#define STATX_CTIME 0x00000080U
#define STATX_INO 0x00000100U
#define STATX_SIZE 0x00000200U
#define STATX_BLOCKS 0x00000400U
#define STATX_BASIC_STATS 0x000007ffU
#define STATX_BTIME 0x00000800U
#endif

static void init_options(struct context *ctx);
static int parse_options(struct context *ctx, int argc, char **argv);
static void parse_long_option(struct context *ctx, const char *arg);
static void require_option_argument(int argc, char **argv, int *index,
    const char *opt_name, const char **value);
static void finalize_options(struct context *ctx);
static size_t parse_size_t_or_default(const char *text, size_t fallback);
static bool follow_root_argument(const struct context *ctx);
static bool follow_child_entry(const struct context *ctx);
static int collect_path_entry(struct context *ctx, struct entry *entry,
    const char *path, const char *name, bool root_argument);
static int stat_with_policy(const struct context *ctx, const char *path,
    bool root_argument, struct entry *entry);
static int fill_birthtime(const char *path, bool follow, struct entry *entry);
static void add_dot_entries(struct context *ctx, struct entry_list *list,
    const char *dir_path);
static bool collect_directory_entries(struct context *ctx,
    struct entry_list *list, const char *dir_path);
static void list_root_files(struct context *ctx, struct entry_list *roots);
static void list_directory(struct context *ctx, const struct entry *dir,
    const char *title, bool print_header, const struct visit_stack *stack,
    int operand_count);
static bool should_recurse(const struct context *ctx, const struct entry *entry);
static bool visit_stack_contains(const struct visit_stack *stack,
    const struct entry *entry);
static void print_directory_header(struct context *ctx, const char *title,
    bool print_header);
static void unsupported_option(const char *opt, const char *reason);
static void warn_path_error(struct context *ctx, const char *path, int error);
static int linux_statx(const char *path, int flags, struct statx *stx);

static void
init_options(struct context *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->now = time(NULL);
	ctx->opt.layout = isatty(STDOUT_FILENO) ? LAYOUT_COLUMNS : LAYOUT_SINGLE;
	ctx->opt.sort = SORT_NAME;
	ctx->opt.time_field = TIME_MTIME;
	ctx->opt.follow = FOLLOW_DEFAULT;
	ctx->opt.group_dirs = GROUP_NONE;
	ctx->opt.name_mode = isatty(STDOUT_FILENO) ? NAME_PRINTABLE : NAME_LITERAL;
	ctx->opt.color_mode = COLOR_DEFAULT;
	ctx->opt.block_units = 1;
	ctx->opt.stdout_is_tty = isatty(STDOUT_FILENO);
	ctx->opt.terminal_width = 80;
}

static size_t
parse_size_t_or_default(const char *text, size_t fallback)
{
	size_t value;
	unsigned char ch;

	if (text == NULL || *text == '\0')
		return (fallback);
	value = 0;
	while (*text != '\0') {
		ch = (unsigned char)*text++;
		if (!isdigit(ch))
			return (fallback);
		if (value > (SIZE_MAX - (size_t)(ch - '0')) / 10)
			return (fallback);
		value = value * 10 + (size_t)(ch - '0');
	}
	if (value == 0)
		return (fallback);
	return (value);
}

static void
require_option_argument(int argc, char **argv, int *index, const char *opt_name,
    const char **value)
{
	if (++(*index) >= argc)
		errx(2, "option %s requires an argument", opt_name);
	*value = argv[*index];
}

static void
unsupported_option(const char *opt, const char *reason)
{
	errx(2, "option %s is not supported on Linux: %s", opt, reason);
}

static void
parse_long_option(struct context *ctx, const char *arg)
{
	const char *value;

	value = strchr(arg, '=');
	if (value != NULL)
		value++;
	if (strncmp(arg, "--color", 7) == 0 &&
	    (arg[7] == '\0' || arg[7] == '=')) {
		if (value == NULL || strcmp(value, "always") == 0 ||
		    strcmp(value, "yes") == 0 || strcmp(value, "force") == 0)
			ctx->opt.color_mode = COLOR_ALWAYS;
		else if (strcmp(value, "auto") == 0 || strcmp(value, "tty") == 0 ||
		    strcmp(value, "if-tty") == 0)
			ctx->opt.color_mode = COLOR_AUTO;
		else if (strcmp(value, "never") == 0 || strcmp(value, "no") == 0 ||
		    strcmp(value, "none") == 0)
			ctx->opt.color_mode = COLOR_NEVER;
		else
			errx(2, "unsupported --color value '%s' (must be always, auto, or never)",
			    value);
		return;
	}
	if (strncmp(arg, "--group-directories", 19) == 0 &&
	    (arg[19] == '\0' || arg[19] == '=')) {
		if (value == NULL || strcmp(value, "first") == 0)
			ctx->opt.group_dirs = GROUP_DIRS_FIRST;
		else if (strcmp(value, "last") == 0)
			ctx->opt.group_dirs = GROUP_DIRS_LAST;
		else
			errx(2, "unsupported --group-directories value '%s' (must be first or last)",
			    value);
		return;
	}
	if (strcmp(arg, "--group-directories-first") == 0) {
		ctx->opt.group_dirs = GROUP_DIRS_FIRST;
		return;
	}
	usage();
}

static int
parse_options(struct context *ctx, int argc, char **argv)
{
	int i;
	int j;
	const char *value;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || strcmp(argv[i], "-") == 0)
			break;
		if (strcmp(argv[i], "--") == 0) {
			i++;
			break;
		}
		if (strncmp(argv[i], "--", 2) == 0) {
			parse_long_option(ctx, argv[i]);
			continue;
		}
		for (j = 1; argv[i][j] != '\0'; j++) {
			switch (argv[i][j]) {
			case '1':
				ctx->opt.layout = LAYOUT_SINGLE;
				break;
			case 'A':
				ctx->opt.show_almost_all = true;
				break;
			case 'B':
				ctx->opt.name_mode = NAME_OCTAL;
				break;
			case 'C':
				ctx->opt.layout = LAYOUT_COLUMNS;
				ctx->opt.sort_across = false;
				break;
			case 'D':
				if (argv[i][j + 1] != '\0') {
					ctx->opt.time_format = &argv[i][j + 1];
					j = (int)strlen(argv[i]) - 1;
				} else {
					require_option_argument(argc, argv, &i, "-D", &value);
					ctx->opt.time_format = value;
					goto next_option_argument;
				}
				break;
			case 'F':
				ctx->opt.show_type = true;
				ctx->opt.slash_only = false;
				break;
			case 'G':
				ctx->opt.color_mode = COLOR_AUTO;
				break;
			case 'H':
				ctx->opt.follow = FOLLOW_COMMAND_LINE;
				break;
			case 'I':
				ctx->opt.disable_root_almost_all = true;
				break;
			case 'L':
				ctx->opt.follow = FOLLOW_ALL;
				break;
			case 'P':
				ctx->opt.follow = FOLLOW_NEVER;
				break;
			case 'R':
				ctx->opt.recursive = true;
				break;
			case 'S':
				ctx->opt.sort = SORT_SIZE;
				break;
			case 'T':
				ctx->opt.show_full_time = true;
				break;
			case 'U':
				ctx->opt.time_field = TIME_BTIME;
				break;
			case 'W':
				unsupported_option("-W", "Linux VFS has no FreeBSD whiteout entries");
				break;
			case 'Z':
				unsupported_option("-Z", "FreeBSD MAC labels do not map to a portable Linux userland interface");
				break;
			case 'a':
				ctx->opt.show_all = true;
				ctx->opt.show_almost_all = false;
				break;
			case 'b':
				ctx->opt.name_mode = NAME_ESCAPE;
				break;
			case 'c':
				ctx->opt.time_field = TIME_CTIME;
				break;
			case 'd':
				ctx->opt.list_directory_itself = true;
				ctx->opt.recursive = false;
				break;
			case 'f':
				ctx->opt.no_sort = true;
				ctx->opt.show_all = true;
				ctx->opt.show_almost_all = false;
				break;
			case 'g':
				ctx->opt.layout = LAYOUT_LONG;
				ctx->opt.suppress_owner = true;
				break;
			case 'h':
				ctx->opt.human_readable = true;
				ctx->opt.block_units = 1;
				break;
			case 'i':
				ctx->opt.show_inode = true;
				break;
			case 'k':
				ctx->opt.human_readable = false;
				ctx->opt.block_units = 2;
				break;
			case 'l':
				ctx->opt.layout = LAYOUT_LONG;
				break;
			case 'm':
				ctx->opt.layout = LAYOUT_STREAM;
				break;
			case 'n':
				ctx->opt.numeric_ids = true;
				ctx->opt.layout = LAYOUT_LONG;
				break;
			case 'o':
				unsupported_option("-o", "FreeBSD file flags require a filesystem-specific ioctl surface on Linux; use lsattr for ext-family flags");
				break;
			case 'p':
				ctx->opt.show_type = true;
				ctx->opt.slash_only = true;
				break;
			case 'q':
				ctx->opt.name_mode = NAME_PRINTABLE;
				break;
			case 'r':
				ctx->opt.reverse_sort = true;
				break;
			case 's':
				ctx->opt.show_blocks = true;
				break;
			case 't':
				ctx->opt.sort = SORT_TIME;
				break;
			case 'u':
				ctx->opt.time_field = TIME_ATIME;
				break;
			case 'v':
				ctx->opt.sort = SORT_VERSION;
				break;
			case 'w':
				ctx->opt.name_mode = NAME_LITERAL;
				break;
			case 'x':
				ctx->opt.layout = LAYOUT_COLUMNS;
				ctx->opt.sort_across = true;
				break;
			case 'y':
				ctx->opt.same_sort_direction = true;
				break;
			case ',':
				ctx->opt.thousands = true;
				break;
			default:
				usage();
			}
		}
next_option_argument:
		;
	}
	return (i);
}

static void
finalize_options(struct context *ctx)
{
	const char *columns;
	const char *clicolor_force;
	const char *clicolor;
	const char *colorterm;
	const char *same_sort;
	struct winsize win;
	bool env_color;
	bool force_color;

	columns = getenv("COLUMNS");
	ctx->opt.terminal_width = parse_size_t_or_default(columns, 80);
	if (columns == NULL && ctx->opt.stdout_is_tty &&
	    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == 0 && win.ws_col > 0)
		ctx->opt.terminal_width = win.ws_col;
	same_sort = getenv("LS_SAMESORT");
	if (same_sort != NULL && *same_sort != '\0')
		ctx->opt.same_sort_direction = true;
	if (!ctx->opt.show_all && geteuid() == 0 && !ctx->opt.disable_root_almost_all)
		ctx->opt.show_almost_all = true;
	clicolor = getenv("CLICOLOR");
	colorterm = getenv("COLORTERM");
	clicolor_force = getenv("CLICOLOR_FORCE");
	env_color = (clicolor != NULL) || (colorterm != NULL && *colorterm != '\0');
	force_color = clicolor_force != NULL && *clicolor_force != '\0';
	switch (ctx->opt.color_mode) {
	case COLOR_NEVER:
		ctx->opt.colorize = false;
		break;
	case COLOR_ALWAYS:
		ctx->opt.colorize = true;
		break;
	case COLOR_AUTO:
		ctx->opt.colorize = ctx->opt.stdout_is_tty || force_color;
		break;
	case COLOR_DEFAULT:
	default:
		ctx->opt.colorize = env_color && (ctx->opt.stdout_is_tty || force_color);
		break;
	}
}

static bool
follow_root_argument(const struct context *ctx)
{
	switch (ctx->opt.follow) {
	case FOLLOW_COMMAND_LINE:
		return (true);
	case FOLLOW_ALL:
		return (true);
	case FOLLOW_NEVER:
		return (false);
	case FOLLOW_DEFAULT:
	default:
		return (!ctx->opt.list_directory_itself && ctx->opt.layout != LAYOUT_LONG &&
		    (!ctx->opt.show_type || ctx->opt.slash_only));
	}
}

static bool
follow_child_entry(const struct context *ctx)
{
	return (ctx->opt.follow == FOLLOW_ALL);
}

static int
linux_statx(const char *path, int flags, struct statx *stx)
{
#ifdef SYS_statx
	return ((int)syscall(SYS_statx, AT_FDCWD, path, flags,
	    STATX_BASIC_STATS | STATX_BTIME, stx));
#else
	(void)path;
	(void)flags;
	(void)stx;
	errno = ENOSYS;
	return (-1);
#endif
}

static int
fill_birthtime(const char *path, bool follow, struct entry *entry)
{
	struct statx stx;
	int flags;

	memset(&stx, 0, sizeof(stx));
	flags = follow ? 0 : AT_SYMLINK_NOFOLLOW;
	if (linux_statx(path, flags, &stx) != 0)
		return (-1);
	if ((stx.stx_mask & STATX_BTIME) == 0)
		return (-1);
	entry->btime.ts.tv_sec = stx.stx_btime.tv_sec;
	entry->btime.ts.tv_nsec = stx.stx_btime.tv_nsec;
	entry->btime.available = true;
	return (0);
}

static int
stat_with_policy(const struct context *ctx, const char *path, bool root_argument,
    struct entry *entry)
{
	bool follow;
	int error;

	follow = root_argument ? follow_root_argument(ctx) : follow_child_entry(ctx);
	entry->followed = follow;
	if ((follow ? stat(path, &entry->st) : lstat(path, &entry->st)) != 0) {
		entry->stat_errno = errno;
		entry->stat_ok = false;
		return (-1);
	}
	entry->stat_ok = true;
	entry->is_dir = S_ISDIR(entry->st.st_mode);
	entry->is_symlink = S_ISLNK(entry->st.st_mode);
	if (ctx->opt.time_field == TIME_BTIME) {
		error = fill_birthtime(path, follow, entry);
		if (error != 0)
			entry->btime.available = false;
	}
	return (0);
}

static int
collect_path_entry(struct context *ctx, struct entry *entry, const char *path,
    const char *name, bool root_argument)
{
	memset(entry, 0, sizeof(*entry));
	entry->name = xstrdup(name);
	entry->path = xstrdup(path);
	if (stat_with_policy(ctx, path, root_argument, entry) != 0) {
		if (entry->stat_errno == 0)
			entry->stat_errno = errno;
		return (-1);
	}
	return (0);
}

static void
warn_path_error(struct context *ctx, const char *path, int error)
{
	warnx("%s: %s", path, strerror(error));
	ctx->exit_status = 1;
}

static void
add_dot_entries(struct context *ctx, struct entry_list *list, const char *dir_path)
{
	struct entry entry;
	char *path;

	path = join_path(dir_path, ".");
	if (collect_path_entry(ctx, &entry, path, ".", false) == 0)
		append_entry(list, &entry);
	else {
		warn_path_error(ctx, path, entry.stat_errno);
		free_entry(&entry);
	}
	free(path);
	path = join_path(dir_path, "..");
	if (collect_path_entry(ctx, &entry, path, "..", false) == 0)
		append_entry(list, &entry);
	else {
		warn_path_error(ctx, path, entry.stat_errno);
		free_entry(&entry);
	}
	free(path);
}

static bool
collect_directory_entries(struct context *ctx, struct entry_list *list,
    const char *dir_path)
{
	DIR *dirp;
	struct dirent *dent;
	struct entry entry;
	char *path;

	dirp = opendir(dir_path);
	if (dirp == NULL) {
		warn_path_error(ctx, dir_path, errno);
		return (false);
	}
	if (ctx->opt.show_all)
		add_dot_entries(ctx, list, dir_path);
	while ((dent = readdir(dirp)) != NULL) {
		if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
			continue;
		if (!ctx->opt.show_all && !ctx->opt.show_almost_all &&
		    dent->d_name[0] == '.')
			continue;
		path = join_path(dir_path, dent->d_name);
		if (collect_path_entry(ctx, &entry, path, dent->d_name, false) == 0)
			append_entry(list, &entry);
		else {
			warn_path_error(ctx, path, entry.stat_errno);
			free_entry(&entry);
		}
		free(path);
	}
	closedir(dirp);
	return (true);
}

static void
print_directory_header(struct context *ctx, const char *title, bool print_header)
{
	if (!print_header)
		return;
	if (ctx->wrote_output)
		putchar('\n');
	print_name(&ctx->opt, title);
	puts(":");
}

static bool
visit_stack_contains(const struct visit_stack *stack, const struct entry *entry)
{
	while (stack != NULL) {
		if (stack->dev == entry->st.st_dev && stack->ino == entry->st.st_ino)
			return (true);
		stack = stack->parent;
	}
	return (false);
}

static bool
should_recurse(const struct context *ctx, const struct entry *entry)
{
	(void)ctx;
	if (!entry->is_dir)
		return (false);
	if (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0)
		return (false);
	return (true);
}

static void
list_directory(struct context *ctx, const struct entry *dir, const char *title,
    bool print_header, const struct visit_stack *stack, int operand_count)
{
	struct entry_list list;
	struct visit_stack here;
	size_t i;
	char *child_title;
	bool child_header;

	(void)operand_count;
	memset(&list, 0, sizeof(list));
	if (!collect_directory_entries(ctx, &list, dir->path))
		return;
	sort_entries(ctx, &list, false);
	print_directory_header(ctx, title, print_header);
	print_entries(ctx, &list, true);
	if (!ctx->opt.recursive) {
		free_entry_list(&list);
		return;
	}
	here.dev = dir->st.st_dev;
	here.ino = dir->st.st_ino;
	here.parent = stack;
	for (i = 0; i < list.len; i++) {
		if (!should_recurse(ctx, &list.items[i]))
			continue;
		if (visit_stack_contains(&here, &list.items[i])) {
			warnx("%s: directory causes a cycle", list.items[i].path);
			ctx->exit_status = 1;
			continue;
		}
		child_title = join_path(title, list.items[i].name);
		child_header = true;
		list_directory(ctx, &list.items[i], child_title, child_header, &here,
		    operand_count);
		free(child_title);
	}
	free_entry_list(&list);
}

static void
list_root_files(struct context *ctx, struct entry_list *roots)
{
	struct entry_list files;
	size_t i;

	memset(&files, 0, sizeof(files));
	for (i = 0; i < roots->len; i++) {
		if (!roots->items[i].is_dir || ctx->opt.list_directory_itself) {
			if (files.len == files.cap) {
				files.cap = files.cap == 0 ? 8 : files.cap * 2;
				files.items = xreallocarray(files.items, files.cap,
				    sizeof(*files.items));
			}
			files.items[files.len++] = roots->items[i];
		}
	}
	if (files.len == 0) {
		free(files.items);
		return;
	}
	sort_entries(ctx, &files, true);
	print_entries(ctx, &files, false);
	free(files.items);
}

int
main(int argc, char **argv)
{
	struct context ctx;
	struct entry_list roots;
	struct entry entry;
	int argi;
	int operand_count;
	size_t i;
	bool print_header;

	setlocale(LC_ALL, "");
	init_options(&ctx);
	argi = parse_options(&ctx, argc, argv);
	finalize_options(&ctx);
	memset(&roots, 0, sizeof(roots));
	operand_count = argc - argi;
	if (operand_count == 0)
		operand_count = 1;
	if (argi == argc) {
		if (collect_path_entry(&ctx, &entry, ".", ".", true) == 0)
			append_entry(&roots, &entry);
		else {
			warn_path_error(&ctx, ".", entry.stat_errno);
			free_entry(&entry);
		}
	} else {
		for (; argi < argc; argi++) {
			if (collect_path_entry(&ctx, &entry, argv[argi], argv[argi], true) == 0)
				append_entry(&roots, &entry);
			else {
				warn_path_error(&ctx, argv[argi], entry.stat_errno);
				free_entry(&entry);
			}
		}
	}
	sort_entries(&ctx, &roots, true);
	list_root_files(&ctx, &roots);
	for (i = 0; i < roots.len; i++) {
		if (!roots.items[i].is_dir || ctx.opt.list_directory_itself)
			continue;
		print_header = operand_count > 1;
		list_directory(&ctx, &roots.items[i], roots.items[i].name,
		    print_header, NULL, operand_count);
	}
	free_entry_list(&roots);
	return (ctx.exit_status);
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: ls [-ABCFGHILPRSTUabcdfghiklmnopqrstuvwxy1,] [--color=when] "
	    "[--group-directories=first|last] [-D format] [file ...]\n");
	exit(1);
}
