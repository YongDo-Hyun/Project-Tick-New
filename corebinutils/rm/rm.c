/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2026
 *	Project Tick. All rights reserved.
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
	const char *progname;
	bool allow_directories;
	bool force;
	bool interactive;
	bool interactive_once;
	bool recursive;
	bool verbose;
	bool one_file_system;
	bool stdin_is_tty;
	int exit_status;
} options_t;

static void usage_rm(void);
static void usage_unlink(void);
static void set_error(options_t *options, const char *path, const char *message);
static void set_errno_error(options_t *options, const char *path);
static const char *program_basename(const char *path);
static bool is_root_operand(const char *path);
static bool is_dot_operand(const char *path);
static bool should_skip_operand(options_t *options, const char *path);
static bool prompt_yesno(const char *fmt, ...);
static bool prompt_once(options_t *options, char **paths);
static bool path_is_writable(const char *path);
static bool prompt_for_removal(const options_t *options, const char *display,
    const struct stat *st, bool is_directory);
static bool prompt_for_directory_descent(const options_t *options,
    const char *display);
static void print_removed(const options_t *options, const char *path);
static char *join_path(const char *base, const char *name);
static int remove_path_at(options_t *options, int parentfd, const char *name,
    const char *display, const struct stat *known_st, dev_t root_dev,
    bool top_level);
static int remove_path(options_t *options, const char *path);
static int remove_simple_path(options_t *options, const char *path);
static int run_unlink_mode(const char *path);

static void
usage_rm(void)
{
	fprintf(stderr, "%s\n", "usage: rm [-f | -i] [-dIPRrvWx] file ...");
	exit(2);
}

static void
usage_unlink(void)
{
	fprintf(stderr, "%s\n", "usage: unlink [--] file");
	exit(2);
}

static void
set_error(options_t *options, const char *path, const char *message)
{
	fprintf(stderr, "%s: %s: %s\n", options->progname, path, message);
	options->exit_status = 1;
}

static void
set_errno_error(options_t *options, const char *path)
{
	set_error(options, path, strerror(errno));
}

static const char *
program_basename(const char *path)
{
	const char *slash;

	slash = strrchr(path, '/');
	return slash == NULL ? path : slash + 1;
}

static bool
is_root_operand(const char *path)
{
	size_t i;

	if (path[0] == '\0') {
		return false;
	}

	for (i = 0; path[i] != '\0'; ++i) {
		if (path[i] != '/') {
			return false;
		}
	}

	return true;
}

static bool
is_dot_operand(const char *path)
{
	size_t len;
	size_t end;
	size_t start;

	len = strlen(path);
	if (len == 0) {
		return false;
	}

	end = len;
	while (end > 1 && path[end - 1] == '/') {
		--end;
	}
	start = end;
	while (start > 0 && path[start - 1] != '/') {
		--start;
	}

	if (end - start == 1 && path[start] == '.') {
		return true;
	}
	if (end - start == 2 && path[start] == '.' && path[start + 1] == '.') {
		return true;
	}

	return false;
}

static bool
should_skip_operand(options_t *options, const char *path)
{
	if (is_root_operand(path)) {
		fprintf(stderr, "%s: \"/\" may not be removed\n", options->progname);
		options->exit_status = 1;
		return true;
	}

	if (is_dot_operand(path)) {
		fprintf(stderr, "%s: \".\" and \"..\" may not be removed\n",
		    options->progname);
		options->exit_status = 1;
		return true;
	}

	return false;
}

static bool
prompt_yesno(const char *fmt, ...)
{
	char buffer[64];
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fflush(stderr);

	if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
		clearerr(stdin);
		return false;
	}

	return buffer[0] == 'y' || buffer[0] == 'Y';
}

static bool
prompt_once(options_t *options, char **paths)
{
	int existing_count;
	int directory_count;
	int file_count;
	int i;
	struct stat st;
	const char *single_directory;

	existing_count = 0;
	directory_count = 0;
	file_count = 0;
	single_directory = NULL;

	for (i = 0; paths[i] != NULL; ++i) {
		if (should_skip_operand(options, paths[i])) {
			continue;
		}
		if (lstat(paths[i], &st) != 0) {
			continue;
		}
		++existing_count;
		if (S_ISDIR(st.st_mode)) {
			++directory_count;
			single_directory = paths[i];
		} else {
			++file_count;
		}
	}

	if (directory_count > 0 && options->recursive) {
		if (directory_count == 1 && file_count == 0) {
			return prompt_yesno("recursively remove %s? ", single_directory);
		}
		if (directory_count == 1) {
			return prompt_yesno("recursively remove %s and %d file%s? ",
			    single_directory, file_count, file_count == 1 ? "" : "s");
		}
		return prompt_yesno("recursively remove %d dir%s and %d file%s? ",
		    directory_count, directory_count == 1 ? "" : "s", file_count,
		    file_count == 1 ? "" : "s");
	}

	if (existing_count > 3) {
		return prompt_yesno("remove %d files? ", existing_count);
	}

	return true;
}

static bool
path_is_writable(const char *path)
{
	return access(path, W_OK) == 0;
}

static bool
prompt_for_removal(const options_t *options, const char *display,
    const struct stat *st, bool is_directory)
{
	if (options->force) {
		return true;
	}

	if (options->interactive) {
		if (is_directory) {
			return prompt_yesno("remove directory %s? ", display);
		}
		return prompt_yesno("remove %s? ", display);
	}

	if (!options->stdin_is_tty || S_ISLNK(st->st_mode) || path_is_writable(display)) {
		return true;
	}

	if (is_directory) {
		return prompt_yesno("override write protection for directory %s? ",
		    display);
	}
	return prompt_yesno("override write protection for %s? ", display);
}

static bool
prompt_for_directory_descent(const options_t *options, const char *display)
{
	if (options->force) {
		return true;
	}

	if (options->interactive) {
		return prompt_yesno("descend into directory %s? ", display);
	}

	if (!options->stdin_is_tty || path_is_writable(display)) {
		return true;
	}

	return prompt_yesno("override write protection for directory %s? ",
	    display);
}

static void
print_removed(const options_t *options, const char *path)
{
	if (!options->verbose) {
		return;
	}
	printf("%s\n", path);
}

static char *
join_path(const char *base, const char *name)
{
	size_t base_len;
	size_t name_len;
	bool add_slash;
	char *result;

	base_len = strlen(base);
	while (base_len > 1 && base[base_len - 1] == '/') {
		--base_len;
	}
	name_len = strlen(name);
	add_slash = base_len > 0 && base[base_len - 1] != '/';

	result = malloc(base_len + (add_slash ? 1U : 0U) + name_len + 1U);
	if (result == NULL) {
		perror("rm: malloc");
		exit(1);
	}

	memcpy(result, base, base_len);
	if (add_slash) {
		result[base_len] = '/';
		memcpy(result + base_len + 1, name, name_len + 1);
	} else {
		memcpy(result + base_len, name, name_len + 1);
	}

	return result;
}

static int
remove_path_at(options_t *options, int parentfd, const char *name,
    const char *display, const struct stat *known_st, dev_t root_dev,
    bool top_level)
{
	struct stat st;
	const struct stat *stp;
	int dirfd;
	DIR *dir;
	struct dirent *entry;
	int child_status;

	if (known_st != NULL) {
		st = *known_st;
		stp = &st;
	} else {
		if (fstatat(parentfd, name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
			if (options->force && errno == ENOENT) {
				return 0;
			}
			set_errno_error(options, display);
			return -1;
		}
		stp = &st;
	}

	if (S_ISDIR(stp->st_mode)) {
		if (!options->recursive) {
			if (!options->allow_directories) {
				set_error(options, display, "is a directory");
				return -1;
			}
			if (!prompt_for_removal(options, display, stp, true)) {
				return 0;
			}
			if (unlinkat(parentfd, name, AT_REMOVEDIR) != 0) {
				if (!(options->force && errno == ENOENT)) {
					set_errno_error(options, display);
					return -1;
				}
			} else {
				print_removed(options, display);
			}
			return 0;
		}

		if (!top_level && options->one_file_system && stp->st_dev != root_dev) {
			return 0;
		}

		if (!prompt_for_directory_descent(options, display)) {
			return 0;
		}

		dirfd = openat(parentfd, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (dirfd >= 0) {
			dir = fdopendir(dirfd);
			if (dir == NULL) {
				close(dirfd);
				set_errno_error(options, display);
				return -1;
			}

			errno = 0;
			while ((entry = readdir(dir)) != NULL) {
				char *child_display;
				struct stat child_st;

				if (strcmp(entry->d_name, ".") == 0 ||
				    strcmp(entry->d_name, "..") == 0) {
					continue;
				}

				child_display = join_path(display, entry->d_name);
				if (fstatat(dirfd, entry->d_name, &child_st,
				    AT_SYMLINK_NOFOLLOW) != 0) {
					if (!(options->force && errno == ENOENT)) {
						set_errno_error(options, child_display);
					}
					free(child_display);
					continue;
				}
				child_status = remove_path_at(options, dirfd, entry->d_name,
				    child_display, &child_st, root_dev, false);
				(void)child_status;
				free(child_display);
			}

			if (errno != 0) {
				set_errno_error(options, display);
			}
			closedir(dir);
		}

		if (options->interactive &&
		    !prompt_for_removal(options, display, stp, true)) {
			return 0;
		}

		if (unlinkat(parentfd, name, AT_REMOVEDIR) != 0) {
			if (options->force && errno == ENOENT) {
				return 0;
			}
			set_errno_error(options, display);
			return -1;
		}
		print_removed(options, display);
		return 0;
	}

	if (!prompt_for_removal(options, display, stp, false)) {
		return 0;
	}

	if (unlinkat(parentfd, name, 0) != 0) {
		if (!(options->force && errno == ENOENT)) {
			set_errno_error(options, display);
			return -1;
		}
		return 0;
	}

	print_removed(options, display);
	return 0;
}

static int
remove_path(options_t *options, const char *path)
{
	struct stat st;

	if (lstat(path, &st) != 0) {
		if (options->force && errno == ENOENT) {
			return 0;
		}
		set_errno_error(options, path);
		return -1;
	}

	return remove_path_at(options, AT_FDCWD, path, path, &st, st.st_dev, true);
}

static int
remove_simple_path(options_t *options, const char *path)
{
	struct stat st;

	if (lstat(path, &st) != 0) {
		if (options->force && errno == ENOENT) {
			return 0;
		}
		set_errno_error(options, path);
		return -1;
	}

	if (S_ISDIR(st.st_mode) && !options->allow_directories) {
		set_error(options, path, "is a directory");
		return -1;
	}

	if (!prompt_for_removal(options, path, &st, S_ISDIR(st.st_mode))) {
		return 0;
	}

	if (S_ISDIR(st.st_mode)) {
		if (unlinkat(AT_FDCWD, path, AT_REMOVEDIR) != 0) {
			if (!(options->force && errno == ENOENT)) {
				set_errno_error(options, path);
				return -1;
			}
		} else {
			print_removed(options, path);
		}
		return 0;
	}

	if (unlinkat(AT_FDCWD, path, 0) != 0) {
		if (!(options->force && errno == ENOENT)) {
			set_errno_error(options, path);
			return -1;
		}
		return 0;
	}

	print_removed(options, path);
	return 0;
}

static int
run_unlink_mode(const char *path)
{
	struct stat st;

	if (lstat(path, &st) != 0) {
		fprintf(stderr, "unlink: %s: %s\n", path, strerror(errno));
		return 1;
	}
	if (S_ISDIR(st.st_mode)) {
		fprintf(stderr, "unlink: %s: is a directory\n", path);
		return 1;
	}
	if (unlink(path) != 0) {
		fprintf(stderr, "unlink: %s: %s\n", path, strerror(errno));
		return 1;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	options_t options;
	int ch;
	int i;

	memset(&options, 0, sizeof(options));
	options.progname = program_basename(argv[0]);

	(void)setlocale(LC_ALL, "");

	if (strcmp(options.progname, "unlink") == 0) {
		if (argc == 2) {
			return run_unlink_mode(argv[1]);
		}
		if (argc == 3 && strcmp(argv[1], "--") == 0) {
			return run_unlink_mode(argv[2]);
		}
		usage_unlink();
	}

	while ((ch = getopt(argc, argv, "dfiIPRrvWx")) != -1) {
		switch (ch) {
		case 'd':
			options.allow_directories = true;
			break;
		case 'f':
			options.force = true;
			options.interactive = false;
			break;
		case 'i':
			options.interactive = true;
			options.force = false;
			break;
		case 'I':
			options.interactive_once = true;
			break;
		case 'P':
			break;
		case 'R':
		case 'r':
			options.recursive = true;
			options.allow_directories = true;
			break;
		case 'v':
			options.verbose = true;
			break;
		case 'W':
			fprintf(stderr, "%s: -W is unsupported on Linux: whiteout undelete is unavailable\n",
			    options.progname);
			return 1;
		case 'x':
			options.one_file_system = true;
			break;
		default:
			usage_rm();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		if (options.force) {
			return 0;
		}
		usage_rm();
	}

	options.stdin_is_tty = isatty(STDIN_FILENO) == 1;

	if (options.interactive_once && !prompt_once(&options, argv)) {
		return 1;
	}

	for (i = 0; argv[i] != NULL; ++i) {
		if (should_skip_operand(&options, argv[i])) {
			continue;
		}
		if (options.recursive) {
			(void)remove_path(&options, argv[i]);
		} else {
			(void)remove_simple_path(&options, argv[i]);
		}
	}

	return options.exit_status;
}
