/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
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
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *progname = "realpath";

static void
set_progname(const char *argv0)
{
	const char *slash;

	if (argv0 == NULL || *argv0 == '\0')
		return;

	slash = strrchr(argv0, '/');
	progname = slash != NULL ? slash + 1 : argv0;
	if (*progname == '\0')
		progname = "realpath";
}

static void
warnx_msg(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static void
warn_path_errno(const char *path, int errnum)
{
	fprintf(stderr, "%s: %s: %s\n", progname, path, strerror(errnum));
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-q] [path ...]\n", progname);
	exit(1);
}

static char *
resolve_path(const char *path)
{
	errno = 0;
	return realpath(path, NULL);
}

static bool
print_line(const char *line)
{
	if (fputs(line, stdout) == EOF || fputc('\n', stdout) == EOF) {
		warn_path_errno("stdout", errno);
		return false;
	}
	return true;
}

int
main(int argc, char *argv[])
{
	const char *path;
	char *resolved;
	bool qflag;
	int ch;
	int exit_status;

	set_progname(argv[0]);

	qflag = false;
	exit_status = 0;
	opterr = 0;
	while ((ch = getopt(argc, argv, "q")) != -1) {
		switch (ch) {
		case 'q':
			qflag = true;
			break;
		case '?':
			if (optopt != 0 && optopt != '?')
				warnx_msg("illegal option -- %c", optopt);
			else
				warnx_msg("illegal option");
			usage();
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	path = argc > 0 ? *argv++ : ".";

	do {
		resolved = resolve_path(path);
		if (resolved == NULL) {
			if (!qflag)
				warn_path_errno(path, errno);
			exit_status = 1;
			continue;
		}
		if (!print_line(resolved))
			exit_status = 1;
		free(resolved);
	} while ((path = *argv++) != NULL);

	if (fflush(stdout) == EOF) {
		warn_path_errno("stdout", errno);
		exit_status = 1;
	}
	if (ferror(stderr))
		return 1;

	return exit_status;
}
