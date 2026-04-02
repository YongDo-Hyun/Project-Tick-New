#define _POSIX_C_SOURCE 200809L

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1992, 1993
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

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mode.h"

struct mkdir_options {
	bool parents;
	bool verbose;
	bool explicit_mode;
	mode_t final_mode;
};

static const char *progname;

static int	create_component(const char *path,
		    const struct mkdir_options *options, bool last_component,
		    bool allow_existing_dir, mode_t intermediate_umask);
static int	create_parents_path(const char *path,
		    const struct mkdir_options *options);
static int	create_single_path(const char *path,
		    const struct mkdir_options *options);
static mode_t	current_umask(void);
static void	error_errno(const char *fmt, ...);
static void	error_msg(const char *fmt, ...);
static int	existing_directory(const char *path);
static int	mkdir_with_umask(const char *path, mode_t mode, mode_t mask);
static int	print_verbose_path(const char *path);
static const char *program_name(const char *argv0);
static char	*xstrdup(const char *text);
static void	usage(void) __attribute__((noreturn));

static const char *
program_name(const char *argv0)
{
	const char *name;

	if (argv0 == NULL || argv0[0] == '\0')
		return ("mkdir");
	name = strrchr(argv0, '/');
	return (name == NULL ? argv0 : name + 1);
}

static void
verror_message(bool with_errno, const char *fmt, va_list ap)
{
	int saved_errno;

	saved_errno = errno;
	(void)fprintf(stderr, "%s: ", progname);
	(void)vfprintf(stderr, fmt, ap);
	if (with_errno)
		(void)fprintf(stderr, ": %s", strerror(saved_errno));
	(void)fputc('\n', stderr);
}

static void
error_errno(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror_message(true, fmt, ap);
	va_end(ap);
}

static void
error_msg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror_message(false, fmt, ap);
	va_end(ap);
}

static char *
xstrdup(const char *text)
{
	size_t len;
	char *copy;

	len = strlen(text) + 1;
	copy = malloc(len);
	if (copy == NULL) {
		error_msg("out of memory");
		exit(1);
	}
	memcpy(copy, text, len);
	return (copy);
}

static mode_t
current_umask(void)
{
	sigset_t all_signals, original_signals;
	mode_t mask;

	sigfillset(&all_signals);
	(void)sigprocmask(SIG_BLOCK, &all_signals, &original_signals);
	mask = umask(0);
	(void)umask(mask);
	(void)sigprocmask(SIG_SETMASK, &original_signals, NULL);
	return (mask);
}

static int
mkdir_with_umask(const char *path, mode_t mode, mode_t mask)
{
	mode_t original_mask;
	int saved_errno;
	int rc;

	original_mask = umask(mask);
	rc = mkdir(path, mode);
	saved_errno = errno;
	(void)umask(original_mask);
	errno = saved_errno;
	return (rc);
}

static int
existing_directory(const char *path)
{
	struct stat st;

	if (stat(path, &st) == -1)
		return (-1);
	return (S_ISDIR(st.st_mode) ? 1 : 0);
}

static int
print_verbose_path(const char *path)
{
	if (printf("%s\n", path) < 0) {
		error_errno("stdout");
		return (1);
	}
	return (0);
}

static int
create_component(const char *path, const struct mkdir_options *options,
    bool last_component, bool allow_existing_dir, mode_t intermediate_umask)
{
	int rc;

	if (last_component) {
		mode_t mode;

		mode = options->explicit_mode ? options->final_mode :
		    (S_IRWXU | S_IRWXG | S_IRWXO);
		rc = mkdir(path, mode);
	} else {
		rc = mkdir_with_umask(path, S_IRWXU | S_IRWXG | S_IRWXO,
		    intermediate_umask);
	}

	if (rc == -1) {
		int dir_status;

		if (errno == EEXIST || errno == EISDIR) {
			dir_status = existing_directory(path);
			if (dir_status == -1) {
				error_errno("%s", path);
				return (1);
			}
			if (allow_existing_dir && dir_status == 1)
				return (0);
			errno = last_component ? EEXIST : ENOTDIR;
		}
		error_errno("%s", path);
		return (1);
	}

	if (last_component && options->explicit_mode &&
	    chmod(path, options->final_mode) == -1) {
		error_errno("%s", path);
		return (1);
	}

	if (options->verbose)
		return (print_verbose_path(path));
	return (0);
}

static int
create_single_path(const char *path, const struct mkdir_options *options)
{
	return (create_component(path, options, true, false, 0));
}

static int
create_parents_path(const char *path, const struct mkdir_options *options)
{
	char *cursor, *path_copy, *segment_end;
	mode_t intermediate_umask;
	int dir_status;
	int status;

	if (path[0] == '\0')
		return (create_single_path(path, options));

	path_copy = xstrdup(path);
	while (path_copy[0] != '\0' && strcmp(path_copy, "/") != 0) {
		size_t len;

		len = strlen(path_copy);
		if (len <= 1 || path_copy[len - 1] != '/')
			break;
		path_copy[len - 1] = '\0';
	}

	dir_status = existing_directory(path_copy);
	if (dir_status == 1) {
		free(path_copy);
		return (0);
	}
	if (dir_status == -1 && errno != ENOENT) {
		error_errno("%s", path_copy);
		free(path_copy);
		return (1);
	}

	intermediate_umask = current_umask() & ~(S_IWUSR | S_IXUSR);
	status = 0;
	cursor = path_copy;
	for (;;) {
		while (*cursor == '/')
			cursor++;
		if (*cursor == '\0')
			break;

		segment_end = cursor;
		while (*segment_end != '\0' && *segment_end != '/')
			segment_end++;

		if (*segment_end == '\0') {
			status = create_component(path_copy, options, true, true,
			    intermediate_umask);
			break;
		}

		*segment_end = '\0';
		status = create_component(path_copy, options, false, true,
		    intermediate_umask);
		*segment_end = '/';
		if (status != 0)
			break;
		cursor = segment_end + 1;
	}

	free(path_copy);
	return (status);
}

int
main(int argc, char *argv[])
{
	struct mkdir_options options;
	void *compiled_mode;
	int ch, exit_status;

	progname = program_name(argv[0]);
	memset(&options, 0, sizeof(options));
	options.final_mode = S_IRWXU | S_IRWXG | S_IRWXO;
	compiled_mode = NULL;

	while ((ch = getopt(argc, argv, "m:pv")) != -1) {
		switch (ch) {
		case 'm':
			mode_free(compiled_mode);
			compiled_mode = mode_compile(optarg);
			if (compiled_mode == NULL)
				error_msg("invalid file mode: %s", optarg);
			if (compiled_mode == NULL)
				return (1);
			options.explicit_mode = true;
			options.final_mode = mode_apply(compiled_mode,
			    S_IRWXU | S_IRWXG | S_IRWXO);
			break;
		case 'p':
			options.parents = true;
			break;
		case 'v':
			options.verbose = true;
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc == 0)
		usage();

	exit_status = 0;
	for (int i = 0; i < argc; i++) {
		int status;

		if (options.parents)
			status = create_parents_path(argv[i], &options);
		else
			status = create_single_path(argv[i], &options);
		if (status != 0)
			exit_status = 1;
	}

	mode_free(compiled_mode);
	return (exit_status);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: mkdir [-pv] [-m mode] directory_name ...\n");
	exit(2);
}
