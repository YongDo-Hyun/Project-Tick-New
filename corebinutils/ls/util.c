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

#include <sys/sysmacros.h>

#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include "ls.h"
#include "extern.h"

static int print_name_literal(const char *name);
static int print_name_printable(const char *name);
static int print_name_octal(const char *name, bool c_escape);
static size_t measure_name_literal(const char *name);
static size_t measure_name_printable(const char *name);
static size_t measure_name_octal(const char *name, bool c_escape);
static char *lookup_user(uid_t uid);
static char *lookup_group(gid_t gid);
static char *humanize_u64(uint64_t value);

void *
xmalloc(size_t size)
{
	void *ptr;

	ptr = malloc(size);
	if (ptr == NULL)
		err(1, "malloc");
	return (ptr);
}

void *
xreallocarray(void *ptr, size_t nmemb, size_t size)
{
	void *new_ptr;

	if (size != 0 && nmemb > SIZE_MAX / size)
		errx(1, "allocation overflow");
	new_ptr = realloc(ptr, nmemb * size);
	if (new_ptr == NULL)
		err(1, "realloc");
	return (new_ptr);
}

char *
xstrdup(const char *src)
{
	char *copy;

	copy = strdup(src);
	if (copy == NULL)
		err(1, "strdup");
	return (copy);
}

char *
xasprintf(const char *fmt, ...)
{
	va_list ap;
	char *buf;
	int len;

	va_start(ap, fmt);
	len = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (len < 0)
		errx(1, "vsnprintf");
	buf = xmalloc((size_t)len + 1);
	va_start(ap, fmt);
	if (vsnprintf(buf, (size_t)len + 1, fmt, ap) != len) {
		va_end(ap);
		errx(1, "vsnprintf");
	}
	va_end(ap);
	return (buf);
}

char *
join_path(const char *parent, const char *name)
{
	size_t parent_len, name_len;
	bool need_sep;
	char *path;

	parent_len = strlen(parent);
	name_len = strlen(name);
	need_sep = parent_len != 0 && parent[parent_len - 1] != '/';
	path = xmalloc(parent_len + (need_sep ? 1 : 0) + name_len + 1);
	memcpy(path, parent, parent_len);
	if (need_sep)
		path[parent_len++] = '/';
	memcpy(path + parent_len, name, name_len + 1);
	return (path);
}

void
free_entry(struct entry *entry)
{
	if (entry == NULL)
		return;
	free(entry->name);
	free(entry->path);
	free(entry->user);
	free(entry->group);
	free(entry->link_target);
	memset(entry, 0, sizeof(*entry));
}

void
free_entry_list(struct entry_list *list)
{
	size_t i;

	if (list == NULL)
		return;
	for (i = 0; i < list->len; i++)
		free_entry(&list->items[i]);
	free(list->items);
	list->items = NULL;
	list->len = 0;
	list->cap = 0;
}

void
append_entry(struct entry_list *list, struct entry *entry)
{
	if (list->len == list->cap) {
		list->cap = list->cap == 0 ? 16 : list->cap * 2;
		list->items = xreallocarray(list->items, list->cap,
		    sizeof(*list->items));
	}
	list->items[list->len++] = *entry;
	memset(entry, 0, sizeof(*entry));
}

size_t
numeric_width(uintmax_t value)
{
	size_t width;

	for (width = 1; value >= 10; width++)
		value /= 10;
	return (width);
}

static int
print_name_literal(const char *name)
{
	mbstate_t state;
	wchar_t wc;
	size_t clen;
	int width;
	int total;

	memset(&state, 0, sizeof(state));
	total = 0;
	while ((clen = mbrtowc(&wc, name, MB_LEN_MAX, &state)) != 0) {
		if (clen == (size_t)-1) {
			memset(&state, 0, sizeof(state));
			putchar((unsigned char)*name++);
			total++;
			continue;
		}
		if (clen == (size_t)-2) {
			total += printf("%s", name);
			break;
		}
		fwrite(name, 1, clen, stdout);
		name += clen;
		width = wcwidth(wc);
		if (width > 0)
			total += width;
	}
	return (total);
}

static int
print_name_printable(const char *name)
{
	mbstate_t state;
	wchar_t wc;
	size_t clen;
	int width;
	int total;

	memset(&state, 0, sizeof(state));
	total = 0;
	while ((clen = mbrtowc(&wc, name, MB_LEN_MAX, &state)) != 0) {
		if (clen == (size_t)-1 || clen == (size_t)-2) {
			putchar('?');
			total++;
			if (clen == (size_t)-1) {
				memset(&state, 0, sizeof(state));
				name++;
			}
			break;
		}
		if (!iswprint(wc)) {
			putchar('?');
			name += clen;
			total++;
			continue;
		}
		fwrite(name, 1, clen, stdout);
		name += clen;
		width = wcwidth(wc);
		if (width > 0)
			total += width;
	}
	return (total);
}

static int
print_name_octal(const char *name, bool c_escape)
{
	static const char escapes[] = "\\\\\"\"\aa\bb\ff\nn\rr\tt\vv";
	mbstate_t state;
	wchar_t wc;
	size_t clen;
	unsigned char ch;
	const char *mapped;
	int printable;
	int total;
	int i;

	memset(&state, 0, sizeof(state));
	total = 0;
	while ((clen = mbrtowc(&wc, name, MB_LEN_MAX, &state)) != 0) {
		printable = clen != (size_t)-1 && clen != (size_t)-2 &&
		    iswprint(wc) && wc != L'\\' && wc != L'\"';
		if (printable) {
			fwrite(name, 1, clen, stdout);
			name += clen;
			i = wcwidth(wc);
			if (i > 0)
				total += i;
			continue;
		}
		if (c_escape && clen != (size_t)-1 && clen != (size_t)-2 &&
#if WCHAR_MIN < 0
		    wc >= 0 &&
#endif
		    wc <= (wchar_t)UCHAR_MAX &&
		    (mapped = strchr(escapes, (char)wc)) != NULL) {
			putchar('\\');
			putchar(mapped[1]);
			name += clen;
			total += 2;
			continue;
		}
		if (clen == (size_t)-2)
			clen = strlen(name);
		else if (clen == (size_t)-1) {
			clen = 1;
			memset(&state, 0, sizeof(state));
		}
		for (i = 0; i < (int)clen; i++) {
			ch = (unsigned char)name[i];
			printf("\\%03o", ch);
			total += 4;
		}
		name += clen;
	}
	return (total);
}

int
print_name(const struct options *opt, const char *name)
{
	switch (opt->name_mode) {
	case NAME_PRINTABLE:
		return (print_name_printable(name));
	case NAME_OCTAL:
		return (print_name_octal(name, false));
	case NAME_ESCAPE:
		return (print_name_octal(name, true));
	case NAME_LITERAL:
	default:
		return (print_name_literal(name));
	}
}

static size_t
measure_name_literal(const char *name)
{
	mbstate_t state;
	wchar_t wc;
	size_t clen;
	size_t total;
	int width;

	memset(&state, 0, sizeof(state));
	total = 0;
	while ((clen = mbrtowc(&wc, name, MB_LEN_MAX, &state)) != 0) {
		if (clen == (size_t)-1) {
			memset(&state, 0, sizeof(state));
			name++;
			total++;
			continue;
		}
		if (clen == (size_t)-2) {
			total += strlen(name);
			break;
		}
		name += clen;
		width = wcwidth(wc);
		if (width > 0)
			total += (size_t)width;
	}
	return (total);
}

static size_t
measure_name_printable(const char *name)
{
	mbstate_t state;
	wchar_t wc;
	size_t clen;
	size_t total;
	int width;

	memset(&state, 0, sizeof(state));
	total = 0;
	while ((clen = mbrtowc(&wc, name, MB_LEN_MAX, &state)) != 0) {
		if (clen == (size_t)-1 || clen == (size_t)-2) {
			total++;
			if (clen == (size_t)-1) {
				memset(&state, 0, sizeof(state));
				name++;
			}
			break;
		}
		if (!iswprint(wc)) {
			name += clen;
			total++;
			continue;
		}
		name += clen;
		width = wcwidth(wc);
		if (width > 0)
			total += (size_t)width;
	}
	return (total);
}

static size_t
measure_name_octal(const char *name, bool c_escape)
{
	static const char escapes[] = "\\\\\"\"\aa\bb\ff\nn\rr\tt\vv";
	mbstate_t state;
	wchar_t wc;
	size_t clen;
	size_t total;
	const char *mapped;
	int width;

	memset(&state, 0, sizeof(state));
	total = 0;
	while ((clen = mbrtowc(&wc, name, MB_LEN_MAX, &state)) != 0) {
		if (clen == (size_t)-2) {
			total += 4 * strlen(name);
			break;
		}
		if (clen == (size_t)-1) {
			memset(&state, 0, sizeof(state));
			name++;
			total += 4;
			continue;
		}
		if (iswprint(wc) && wc != L'\\' && wc != L'\"') {
			width = wcwidth(wc);
			if (width > 0)
				total += (size_t)width;
			name += clen;
			continue;
		}
		if (c_escape &&
#if WCHAR_MIN < 0
		    wc >= 0 &&
#endif
		    wc <= (wchar_t)UCHAR_MAX &&
		    (mapped = strchr(escapes, (char)wc)) != NULL) {
			name += clen;
			total += 2;
			continue;
		}
		name += clen;
		total += 4 * clen;
	}
	return (total);
}

size_t
measure_name(const struct options *opt, const char *name)
{
	switch (opt->name_mode) {
	case NAME_PRINTABLE:
		return (measure_name_printable(name));
	case NAME_OCTAL:
		return (measure_name_octal(name, false));
	case NAME_ESCAPE:
		return (measure_name_octal(name, true));
	case NAME_LITERAL:
	default:
		return (measure_name_literal(name));
	}
}

char
file_type_char(const struct entry *entry, bool slash_only)
{
	mode_t mode;

	mode = entry->st.st_mode;
	if (slash_only)
		return (entry->is_dir ? '/' : '\0');
	if (entry->is_dir)
		return ('/');
	if (S_ISFIFO(mode))
		return ('|');
	if (S_ISLNK(mode))
		return ('@');
	if (S_ISSOCK(mode))
		return ('=');
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		return ('*');
	return ('\0');
}

void
mode_string(const struct entry *entry, char buf[12])
{
	mode_t mode;

	mode = entry->st.st_mode;
	buf[0] = S_ISDIR(mode) ? 'd' :
	    S_ISLNK(mode) ? 'l' :
	    S_ISCHR(mode) ? 'c' :
	    S_ISBLK(mode) ? 'b' :
	    S_ISFIFO(mode) ? 'p' :
	    S_ISSOCK(mode) ? 's' : '-';
	buf[1] = (mode & S_IRUSR) ? 'r' : '-';
	buf[2] = (mode & S_IWUSR) ? 'w' : '-';
	buf[3] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S') :
	    ((mode & S_IXUSR) ? 'x' : '-');
	buf[4] = (mode & S_IRGRP) ? 'r' : '-';
	buf[5] = (mode & S_IWGRP) ? 'w' : '-';
	buf[6] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S') :
	    ((mode & S_IXGRP) ? 'x' : '-');
	buf[7] = (mode & S_IROTH) ? 'r' : '-';
	buf[8] = (mode & S_IWOTH) ? 'w' : '-';
	buf[9] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T') :
	    ((mode & S_IXOTH) ? 'x' : '-');
	buf[10] = ' ';
	buf[11] = '\0';
}

char *
format_uintmax(uintmax_t value, bool thousands)
{
	char tmp[64];
	size_t len;
	size_t out_len;
	size_t commas;
	size_t i;
	char *out;

	snprintf(tmp, sizeof(tmp), "%ju", value);
	if (!thousands)
		return (xstrdup(tmp));
	len = strlen(tmp);
	commas = len > 0 ? (len - 1) / 3 : 0;
	out_len = len + commas;
	out = xmalloc(out_len + 1);
	out[out_len] = '\0';
	for (i = 0; i < len; i++) {
		out[out_len - 1 - i - i / 3] = tmp[len - 1 - i];
		if (i / 3 != (i + 1) / 3 && i + 1 < len)
			out[out_len - 1 - i - i / 3 - 1] = ',';
	}
	return (out);
}

char *
format_block_count(blkcnt_t blocks, long units, bool thousands)
{
	uintmax_t scaled;

	if (blocks < 0)
		blocks = 0;
	if (units <= 0)
		units = 1;
	scaled = ((uintmax_t)blocks + (uintmax_t)units - 1) / (uintmax_t)units;
	return (format_uintmax(scaled, thousands));
}

static char *
humanize_u64(uint64_t value)
{
	static const char suffixes[] = "BKMGTPE";
	double scaled;
	unsigned idx;

	scaled = (double)value;
	idx = 0;
	while (scaled >= 1024.0 && idx + 1 < sizeof(suffixes) - 1) {
		scaled /= 1024.0;
		idx++;
	}
	if (idx == 0)
		return (xasprintf("%" PRIu64 "B", value));
	if (scaled >= 10.0)
		return (xasprintf("%.0f%c", scaled, suffixes[idx]));
	return (xasprintf("%.1f%c", scaled, suffixes[idx]));
}

char *
format_entry_size(const struct entry *entry, bool human, bool thousands)
{
	uint64_t size;

	if (S_ISCHR(entry->st.st_mode) || S_ISBLK(entry->st.st_mode))
		return (xasprintf("%u,%u", major(entry->st.st_rdev),
		    minor(entry->st.st_rdev)));
	size = entry->st.st_size < 0 ? 0 : (uint64_t)entry->st.st_size;
	if (human)
		return (humanize_u64(size));
	return (format_uintmax(size, thousands));
}

static char *
lookup_user(uid_t uid)
{
	struct passwd pwd;
	struct passwd *result;
	char *name;
	char *buf;
	long len;
	int error;

	len = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (len < 1024)
		len = 1024;
	buf = xmalloc((size_t)len);
	while ((error = getpwuid_r(uid, &pwd, buf, (size_t)len, &result)) == ERANGE) {
		len *= 2;
		buf = xreallocarray(buf, (size_t)len, 1);
	}
	if (error != 0 || result == NULL) {
		free(buf);
		return (format_uintmax(uid, false));
	}
	name = xstrdup(pwd.pw_name);
	free(buf);
	return (name);
}

static char *
lookup_group(gid_t gid)
{
	struct group grp;
	struct group *result;
	char *name;
	char *buf;
	long len;
	int error;

	len = sysconf(_SC_GETGR_R_SIZE_MAX);
	if (len < 1024)
		len = 1024;
	buf = xmalloc((size_t)len);
	while ((error = getgrgid_r(gid, &grp, buf, (size_t)len, &result)) == ERANGE) {
		len *= 2;
		buf = xreallocarray(buf, (size_t)len, 1);
	}
	if (error != 0 || result == NULL) {
		free(buf);
		return (format_uintmax(gid, false));
	}
	name = xstrdup(grp.gr_name);
	free(buf);
	return (name);
}

int
ensure_owner_group(struct entry *entry, const struct options *opt)
{
	if (entry->user != NULL && entry->group != NULL)
		return (0);
	free(entry->user);
	free(entry->group);
	if (opt->numeric_ids) {
		entry->user = format_uintmax(entry->st.st_uid, false);
		entry->group = format_uintmax(entry->st.st_gid, false);
	} else {
		entry->user = lookup_user(entry->st.st_uid);
		entry->group = lookup_group(entry->st.st_gid);
	}
	return (0);
}

int
ensure_link_target(struct entry *entry)
{
	ssize_t len;
	size_t cap;
	char *buf;

	if (entry->link_target != NULL || !entry->is_symlink)
		return (0);
	cap = 128;
	buf = xmalloc(cap);
	for (;;) {
		len = readlink(entry->path, buf, cap - 1);
		if (len < 0) {
			free(buf);
			return (-1);
		}
		if ((size_t)len < cap - 1) {
			buf[len] = '\0';
			entry->link_target = buf;
			return (0);
		}
		cap *= 2;
		buf = xreallocarray(buf, cap, 1);
	}
}

bool
selected_time(const struct context *ctx, const struct entry *entry,
    struct timespec *ts)
{
	switch (ctx->opt.time_field) {
	case TIME_ATIME:
		*ts = entry->st.st_atim;
		return (true);
	case TIME_CTIME:
		*ts = entry->st.st_ctim;
		return (true);
	case TIME_BTIME:
		if (entry->btime.available)
			*ts = entry->btime.ts;
		else
			*ts = entry->st.st_mtim;
		return (true);
	case TIME_MTIME:
	default:
		*ts = entry->st.st_mtim;
		return (true);
	}
}

char *
format_entry_time(const struct context *ctx, const struct entry *entry)
{
	struct timespec ts;
	struct tm tm;
	char buf[128];
	const char *fmt;
	bool recent;

	selected_time(ctx, entry, &ts);
	if (localtime_r(&ts.tv_sec, &tm) == NULL)
		return (xstrdup("<bad-time>"));
	if (ctx->opt.time_format != NULL)
		fmt = ctx->opt.time_format;
	else if (ctx->opt.show_full_time)
		fmt = "%b %e %H:%M:%S %Y";
	else {
		recent = ts.tv_sec + (365 / 2) * 86400 > ctx->now &&
		    ts.tv_sec < ctx->now + (365 / 2) * 86400;
		fmt = recent ? "%b %e %H:%M" : "%b %e  %Y";
	}
	if (strftime(buf, sizeof(buf), fmt, &tm) == 0)
		return (xstrdup("<bad-format>"));
	return (xstrdup(buf));
}

const char *
color_start(const struct context *ctx, const struct entry *entry)
{
	mode_t mode;

	if (!ctx->opt.colorize)
		return ("");
	mode = entry->st.st_mode;
	if (S_ISDIR(mode))
		return ("\033[01;34m");
	if (S_ISLNK(mode))
		return ("\033[01;36m");
	if (S_ISFIFO(mode))
		return ("\033[33m");
	if (S_ISSOCK(mode))
		return ("\033[01;35m");
	if (S_ISBLK(mode) || S_ISCHR(mode))
		return ("\033[01;33m");
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		return ("\033[01;32m");
	return ("");
}

const char *
color_end(const struct context *ctx)
{
	return (ctx->opt.colorize ? "\033[0m" : "");
}

int
natural_version_compare(const char *left, const char *right)
{
	const unsigned char *a;
	const unsigned char *b;

	a = (const unsigned char *)left;
	b = (const unsigned char *)right;
	while (*a != '\0' || *b != '\0') {
		if (isdigit(*a) && isdigit(*b)) {
			const unsigned char *a0;
			const unsigned char *b0;
			size_t len_a;
			size_t len_b;
			int cmp;

			a0 = a;
			b0 = b;
			while (*a == '0')
				a++;
			while (*b == '0')
				b++;
			len_a = 0;
			while (isdigit(a[len_a]))
				len_a++;
			len_b = 0;
			while (isdigit(b[len_b]))
				len_b++;
			if (len_a != len_b)
				return (len_a < len_b ? -1 : 1);
			cmp = memcmp(a, b, len_a);
			if (cmp != 0)
				return (cmp < 0 ? -1 : 1);
			while (isdigit(*a0))
				a0++;
			while (isdigit(*b0))
				b0++;
			if ((size_t)(a0 - (const unsigned char *)left) !=
			    (size_t)(b0 - (const unsigned char *)right)) {
				if ((a0 - a) != (b0 - b))
					return ((a0 - a) > (b0 - b) ? -1 : 1);
			}
			a = a0;
			b = b0;
			continue;
		}
		if (*a != *b)
			return (*a < *b ? -1 : 1);
		if (*a != '\0')
			a++;
		if (*b != '\0')
			b++;
	}
	return (0);
}
