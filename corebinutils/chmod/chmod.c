/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 2026
 *  Project Tick. All rights reserved.
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

#include <sys/param.h>
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mode.h"

#ifndef SIGINFO
#define SIGINFO SIGUSR1
#endif

#ifndef NODEV
#define NODEV ((dev_t)-1)
#endif

#ifndef ALLPERMS
#define ALLPERMS (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)
#endif

struct chmod_options {
	int fflag;
	int hflag;
	int recursive;
	int vflag;
	bool root_follow;
	bool follow_links;
};

struct visit {
	dev_t dev;
	ino_t ino;
};

struct visit_set {
	struct visit *items;
	size_t len;
	size_t cap;
};

static volatile sig_atomic_t siginfo;

static void usage(void) __attribute__((noreturn));
static void siginfo_handler(int sig);
static bool stat_path(const char *path, bool follow, struct stat *st);
static bool should_skip_acl_check(const char *path, int hflag, dev_t dev);
static int apply_mode(const char *display_path, const char *apply_path,
    const struct stat *st, const struct chmod_options *opts, void *set,
    bool dereference);
static int walk_path(const char *path, const struct chmod_options *opts,
    void *set, bool dereference, struct visit_set *visited);
static int walk_dir(const char *path, const struct chmod_options *opts,
    void *set, struct visit_set *visited);
static bool visited_contains(const struct visit_set *visited, dev_t dev,
    ino_t ino);
static bool visited_add(struct visit_set *visited, dev_t dev, ino_t ino);
static void visited_pop(struct visit_set *visited);
static char *join_path(const char *parent, const char *name);

static void
siginfo_handler(int sig)
{
	(void)sig;
	siginfo = 1;
}

int
main(int argc, char *argv[])
{
	struct chmod_options opts;
	struct visit_set visited;
	void *set;
	int Hflag, Lflag, Rflag, ch, rval;
	char *mode;
	int i;

	set = NULL;
	memset(&opts, 0, sizeof(opts));
	memset(&visited, 0, sizeof(visited));
	Hflag = Lflag = Rflag = 0;

	while ((ch = getopt(argc, argv, "HLPRXfghorstuvwx")) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = 0;
			break;
		case 'P':
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':
			opts.fflag = 1;
			break;
		case 'h':
			opts.hflag = 1;
			break;
		case 'g': case 'o': case 'r': case 's':
		case 't': case 'u': case 'w': case 'X': case 'x':
			if (argv[optind - 1][0] == '-' &&
			    argv[optind - 1][1] == ch &&
			    argv[optind - 1][2] == '\0')
				--optind;
			goto done;
		case 'v':
			opts.vflag++;
			break;
		case '?':
		default:
			usage();
		}
done:
	argv += optind;
	argc -= optind;

	if (argc < 2)
		usage();

	(void)signal(SIGINFO, siginfo_handler);

	opts.recursive = Rflag;
	if (Rflag) {
		if (opts.hflag)
			errx(1, "the -R and -h options may not be specified together.");
		if (Lflag) {
			opts.root_follow = true;
			opts.follow_links = true;
		} else if (Hflag) {
			opts.root_follow = true;
			opts.follow_links = false;
		} else {
			opts.root_follow = false;
			opts.follow_links = false;
		}
	} else {
		opts.root_follow = (opts.hflag == 0);
		opts.follow_links = false;
	}

	mode = *argv;
	if ((set = mode_compile(mode)) == NULL)
		errx(1, "invalid file mode: %s", mode);

	rval = 0;
	for (i = 1; i < argc; i++) {
		if (walk_path(argv[i], &opts, set, opts.root_follow, &visited) != 0)
			rval = 1;
	}

	mode_free(set);
	free(visited.items);
	return (rval);
}

static int
walk_path(const char *path, const struct chmod_options *opts, void *set,
    bool dereference, struct visit_set *visited)
{
	struct stat st;
	int rval;

	if (!stat_path(path, dereference, &st)) {
		warn("%s", path);
		return (1);
	}

	rval = apply_mode(path, path, &st, opts, set, dereference);
	if (rval != 0 && opts->fflag)
		rval = 0;

	if (!opts->recursive || !S_ISDIR(st.st_mode))
		return (rval);

	if (visited_contains(visited, st.st_dev, st.st_ino)) {
		warnx("%s: directory causes a cycle", path);
		return (1);
	}
	if (!visited_add(visited, st.st_dev, st.st_ino)) {
		if (!opts->fflag)
			warnx("%s: insufficient memory", path);
		return (1);
	}

	if (walk_dir(path, opts, set, visited) != 0)
		rval = 1;
	visited_pop(visited);
	return (rval);
}

static int
walk_dir(const char *path, const struct chmod_options *opts, void *set,
    struct visit_set *visited)
{
	DIR *dirp;
	struct dirent *dp;
	int rval;

	dirp = opendir(path);
	if (dirp == NULL) {
		warn("%s", path);
		return (1);
	}

	rval = 0;
	while ((dp = readdir(dirp)) != NULL) {
		char *child;
		bool dereference;
		struct stat st;

		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		child = join_path(path, dp->d_name);
		if (child == NULL) {
			if (!opts->fflag)
				warnx("%s: insufficient memory", path);
			rval = 1;
			break;
		}

		dereference = opts->follow_links;
		if (opts->follow_links) {
			if (!stat_path(child, true, &st)) {
				warn("%s", child);
				free(child);
				rval = 1;
				continue;
			}
		}

		if (walk_path(child, opts, set, dereference, visited) != 0)
			rval = 1;
		free(child);
	}

	if (closedir(dirp) != 0) {
		warn("%s", path);
		rval = 1;
	}

	return (rval);
}

static int
apply_mode(const char *display_path, const char *apply_path, const struct stat *st,
    const struct chmod_options *opts, void *set, bool dereference)
{
	mode_t newmode;
	int atflag;

	newmode = mode_apply(set, st->st_mode);
	if (should_skip_acl_check(apply_path, opts->hflag, st->st_dev) &&
	    (newmode & ALLPERMS) == (st->st_mode & ALLPERMS))
		return (0);

	atflag = dereference ? 0 : AT_SYMLINK_NOFOLLOW;
	if (fchmodat(AT_FDCWD, apply_path, newmode, atflag) == -1) {
		if (!opts->fflag)
			warn("%s", display_path);
		return (1);
	}

	if (opts->vflag || siginfo) {
		(void)printf("%s", display_path);
		if (opts->vflag > 1 || siginfo) {
			char before[12], after[12];

			strmode(st->st_mode, before);
			strmode((st->st_mode & S_IFMT) | newmode, after);
			(void)printf(": 0%o [%s] -> 0%o [%s]",
			    st->st_mode, before,
			    (st->st_mode & S_IFMT) | newmode, after);
		}
		(void)printf("\n");
		siginfo = 0;
	}

	return (0);
}

static bool
stat_path(const char *path, bool follow, struct stat *st)
{
	if (follow)
		return (stat(path, st) == 0);
	return (lstat(path, st) == 0);
}

static bool
should_skip_acl_check(const char *path, int hflag, dev_t dev)
{
	static dev_t previous_dev = NODEV;
	static int supports_acls = -1;

	if (previous_dev != dev) {
		previous_dev = dev;
		supports_acls = 0;
#ifdef _PC_ACL_NFS4
		{
			int ret;

			if (hflag)
				ret = lpathconf(path, _PC_ACL_NFS4);
			else
				ret = pathconf(path, _PC_ACL_NFS4);
			if (ret > 0)
				supports_acls = 1;
			else if (ret < 0 && errno != EINVAL)
				warn("%s", path);
		}
#else
		(void)hflag;
		(void)path;
#endif
	}

	return (supports_acls == 0);
}

static bool
visited_contains(const struct visit_set *visited, dev_t dev, ino_t ino)
{
	size_t i;

	for (i = 0; i < visited->len; i++) {
		if (visited->items[i].dev == dev && visited->items[i].ino == ino)
			return (true);
	}
	return (false);
}

static bool
visited_add(struct visit_set *visited, dev_t dev, ino_t ino)
{
	struct visit *new_items;

	if (visited->len == visited->cap) {
		size_t new_cap;

		new_cap = visited->cap == 0 ? 16 : visited->cap * 2;
		new_items = reallocarray(visited->items, new_cap,
		    sizeof(*visited->items));
		if (new_items == NULL)
			return (false);
		visited->items = new_items;
		visited->cap = new_cap;
	}

	visited->items[visited->len].dev = dev;
	visited->items[visited->len].ino = ino;
	visited->len++;
	return (true);
}

static void
visited_pop(struct visit_set *visited)
{
	if (visited->len > 0)
		visited->len--;
}

static char *
join_path(const char *parent, const char *name)
{
	char *path;
	size_t parent_len, name_len;
	bool need_slash;

	parent_len = strlen(parent);
	name_len = strlen(name);
	need_slash = parent_len > 0 && parent[parent_len - 1] != '/';
	path = malloc(parent_len + need_slash + name_len + 1);
	if (path == NULL)
		return (NULL);

	memcpy(path, parent, parent_len);
	if (need_slash)
		path[parent_len++] = '/';
	memcpy(path + parent_len, name, name_len + 1);
	return (path);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: chmod [-fhv] [-R [-H | -L | -P]] mode file ...\n");
	exit(1);
}
