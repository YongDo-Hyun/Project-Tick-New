/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1985, 1987, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "vary.h"

#ifndef TM_YEAR_BASE
#define TM_YEAR_BASE 1900
#endif

#define DEFAULT_PLUS_FORMAT "%a %b %e %T %Z %Y"

struct iso8601_fmt {
	const char *refname;
	const char *format_string;
	bool include_zone;
};

struct strbuf {
	char *data;
	size_t len;
	size_t cap;
};

struct options {
	const char *input_format;
	const char *output_zone;
	const char *reference_arg;
	const char *time_operand;
	const char *format_operand;
	struct vary *vary_chain;
	const struct iso8601_fmt *iso8601_selected;
	bool no_set;
	bool rfc2822;
	bool use_utc;
};

static const struct iso8601_fmt iso8601_fmts[] = {
	{ "date", "%Y-%m-%d", false },
	{ "hours", "%Y-%m-%dT%H", true },
	{ "minutes", "%Y-%m-%dT%H:%M", true },
	{ "seconds", "%Y-%m-%dT%H:%M:%S", true },
	{ "ns", "%Y-%m-%dT%H:%M:%S,%N", true },
};

static const char *progname = "date";
static const char *rfc2822_format = "%a, %d %b %Y %T %z";

static void usage(void) __attribute__((__noreturn__));
static void die(const char *fmt, ...) __attribute__((__noreturn__, format(printf, 1, 2)));
static void die_errno(const char *fmt, ...) __attribute__((__noreturn__, format(printf, 1, 2)));

static void strbuf_init(struct strbuf *buf);
static void strbuf_reserve(struct strbuf *buf, size_t extra);
static void strbuf_append_mem(struct strbuf *buf, const char *data, size_t len);
static void strbuf_append_char(struct strbuf *buf, char ch);
static void strbuf_append_str(struct strbuf *buf, const char *text);
static char *strbuf_finish(struct strbuf *buf);

static void set_progname(const char *argv0);
static void parse_args(int argc, char **argv, struct options *options);
static const char *require_option_arg(int argc, char **argv, int *index,
    const char *arg, const char *option_name);
static void parse_iso8601_arg(struct options *options, const char *arg);
static void parse_operands(int argc, char **argv, int start_index,
    struct options *options);
static void validate_options(const struct options *options);
static void set_timezone_or_die(const char *tz_value, const char *what);
static time_t int64_to_time_t(int64_t value, const char *what);
static void read_reference_time(const char *arg, struct timespec *ts);
static void read_current_time(struct timespec *ts, struct timespec *resolution);
static void localtime_or_die(time_t value, struct tm *tm);
static void parse_legacy_time(const char *text, const struct timespec *base,
    struct timespec *ts);
static void parse_formatted_time(const char *format, const char *text,
    const struct timespec *base, struct timespec *ts);
static void parse_time_operand(const struct options *options,
    const struct timespec *base, struct timespec *ts);
static void set_system_time(const struct timespec *ts);
static void apply_variations(const struct options *options, struct tm *tm);
static char *expand_format_string(const char *format, long nsec, long resolution);
static void append_nsec_digits(struct strbuf *buf, const char *pending, size_t len,
    long nsec, long resolution);
static char *render_format(const char *format, const struct tm *tm, long nsec,
    long resolution);
static char *render_iso8601(const struct iso8601_fmt *selection,
    const struct tm *tm, long nsec, long resolution);
static char *render_numeric_timezone(const struct tm *tm);
static void print_line_and_exit(const char *text) __attribute__((__noreturn__));

int
main(int argc, char **argv)
{
	struct options options;
	struct timespec ts;
	struct timespec resolution = { 0, 1 };
	struct tm tm;
	const char *format;
	char *output;

	set_progname(argv[0]);
	parse_args(argc, argv, &options);
	validate_options(&options);

	setlocale(LC_TIME, "");

	if (options.use_utc)
		set_timezone_or_die("UTC0", "TZ=UTC0");

	if (options.reference_arg != NULL)
		read_reference_time(options.reference_arg, &ts);
	else
		read_current_time(&ts, &resolution);

	if (options.time_operand != NULL) {
		parse_time_operand(&options, &ts, &ts);
		if (!options.no_set)
			set_system_time(&ts);
	} else if (options.input_format != NULL) {
		die("option -f requires an input date operand");
	}

	if (options.output_zone != NULL)
		set_timezone_or_die(options.output_zone, "TZ");

	localtime_or_die(ts.tv_sec, &tm);
	apply_variations(&options, &tm);

	if (options.iso8601_selected != NULL) {
		output = render_iso8601(options.iso8601_selected, &tm, ts.tv_nsec,
		    resolution.tv_nsec);
		print_line_and_exit(output);
	}

	if (options.rfc2822) {
		if (setlocale(LC_TIME, "C") == NULL)
			die("failed to activate C locale for -R");
		format = rfc2822_format;
	} else if (options.format_operand != NULL) {
		format = options.format_operand;
	} else {
		format = "%+";
	}

	output = render_format(format, &tm, ts.tv_nsec, resolution.tv_nsec);
	print_line_and_exit(output);
}

static void
strbuf_init(struct strbuf *buf)
{
	buf->data = NULL;
	buf->len = 0;
	buf->cap = 0;
}

static void
strbuf_reserve(struct strbuf *buf, size_t extra)
{
	size_t needed;
	size_t newcap;
	char *newdata;

	needed = buf->len + extra + 1;
	if (needed <= buf->cap)
		return;

	newcap = buf->cap == 0 ? 64 : buf->cap;
	while (newcap < needed) {
		if (newcap > SIZE_MAX / 2)
			die("buffer too large");
		newcap *= 2;
	}

	newdata = realloc(buf->data, newcap);
	if (newdata == NULL)
		die_errno("realloc");

	buf->data = newdata;
	buf->cap = newcap;
}

static void
strbuf_append_mem(struct strbuf *buf, const char *data, size_t len)
{
	strbuf_reserve(buf, len);
	memcpy(buf->data + buf->len, data, len);
	buf->len += len;
	buf->data[buf->len] = '\0';
}

static void
strbuf_append_char(struct strbuf *buf, char ch)
{
	strbuf_reserve(buf, 1);
	buf->data[buf->len++] = ch;
	buf->data[buf->len] = '\0';
}

static void
strbuf_append_str(struct strbuf *buf, const char *text)
{
	strbuf_append_mem(buf, text, strlen(text));
}

static char *
strbuf_finish(struct strbuf *buf)
{
	char *result;

	if (buf->data == NULL) {
		result = strdup("");
		if (result == NULL)
			die_errno("strdup");
		return (result);
	}

	result = buf->data;
	buf->data = NULL;
	buf->len = 0;
	buf->cap = 0;
	return (result);
}

static void
set_progname(const char *argv0)
{
	const char *base;

	if (argv0 == NULL || argv0[0] == '\0')
		return;

	base = strrchr(argv0, '/');
	progname = base != NULL ? base + 1 : argv0;
}

static void
parse_args(int argc, char **argv, struct options *options)
{
	int i;
	int j;

	memset(options, 0, sizeof(*options));

	for (i = 1; i < argc; i++) {
		const char *arg;

		arg = argv[i];
		if (arg[0] != '-' || arg[1] == '\0')
			break;
		if (strcmp(arg, "--") == 0) {
			i++;
			break;
		}

		if (strncmp(arg, "-I", 2) == 0) {
			parse_iso8601_arg(options, arg[2] == '\0' ? NULL : arg + 2);
			continue;
		}
		if (strncmp(arg, "-f", 2) == 0) {
			options->input_format = require_option_arg(argc, argv, &i,
			    arg + 2, "-f");
			continue;
		}
		if (strncmp(arg, "-r", 2) == 0) {
			options->reference_arg = require_option_arg(argc, argv, &i,
			    arg + 2, "-r");
			continue;
		}
		if (strncmp(arg, "-v", 2) == 0) {
			options->vary_chain = vary_append(options->vary_chain,
			    require_option_arg(argc, argv, &i, arg + 2, "-v"));
			continue;
		}
		if (strncmp(arg, "-z", 2) == 0) {
			options->output_zone = require_option_arg(argc, argv, &i,
			    arg + 2, "-z");
			continue;
		}

		for (j = 1; arg[j] != '\0'; j++) {
			switch (arg[j]) {
			case 'j':
				options->no_set = true;
				break;
			case 'n':
				die("option -n is not supported on Linux");
			case 'R':
				options->rfc2822 = true;
				break;
			case 'u':
				options->use_utc = true;
				break;
			default:
				usage();
			}
		}
	}

	parse_operands(argc, argv, i, options);
}

static const char *
require_option_arg(int argc, char **argv, int *index, const char *arg,
    const char *option_name)
{
	if (arg != NULL && arg[0] != '\0')
		return (arg);
	if (*index + 1 >= argc)
		die("option %s requires an argument", option_name);
	(*index)++;
	return (argv[*index]);
}

static void
parse_iso8601_arg(struct options *options, const char *arg)
{
	size_t i;

	options->iso8601_selected = &iso8601_fmts[0];
	if (arg == NULL)
		return;

	for (i = 0; i < sizeof(iso8601_fmts) / sizeof(iso8601_fmts[0]); i++) {
		if (strcmp(arg, iso8601_fmts[i].refname) == 0) {
			options->iso8601_selected = &iso8601_fmts[i];
			return;
		}
	}

	die("invalid argument '%s' for -I", arg);
}

static void
parse_operands(int argc, char **argv, int start_index, struct options *options)
{
	int i;

	for (i = start_index; i < argc; i++) {
		if (argv[i][0] == '+') {
			if (options->format_operand != NULL)
				usage();
			options->format_operand = argv[i] + 1;
			continue;
		}
		if (options->time_operand != NULL)
			usage();
		options->time_operand = argv[i];
	}
}

static void
validate_options(const struct options *options)
{
	if (options->iso8601_selected != NULL && options->rfc2822)
		die("multiple output formats specified");
	if (options->iso8601_selected != NULL && options->format_operand != NULL)
		die("multiple output formats specified");
	if (options->rfc2822 && options->format_operand != NULL)
		die("multiple output formats specified");
}

static void
set_timezone_or_die(const char *tz_value, const char *what)
{
	if (setenv("TZ", tz_value, 1) != 0)
		die_errno("setenv(%s)", what);
	tzset();
}

static time_t
int64_to_time_t(int64_t value, const char *what)
{
	time_t converted;

	converted = (time_t)value;
	if ((int64_t)converted != value)
		die("%s out of range: %" PRId64, what, value);
	return (converted);
}

static void
read_reference_time(const char *arg, struct timespec *ts)
{
	struct stat sb;
	char *end;
	long long seconds;

	errno = 0;
	seconds = strtoll(arg, &end, 10);
	if (errno == 0 && arg[0] != '\0' && *end == '\0') {
		ts->tv_sec = int64_to_time_t((int64_t)seconds, "seconds");
		ts->tv_nsec = 0;
		return;
	}

	if (stat(arg, &sb) != 0)
		die_errno("%s", arg);

	ts->tv_sec = sb.st_mtim.tv_sec;
	ts->tv_nsec = sb.st_mtim.tv_nsec;
}

static void
read_current_time(struct timespec *ts, struct timespec *resolution)
{
	if (clock_gettime(CLOCK_REALTIME, ts) != 0)
		die_errno("clock_gettime");
	if (clock_getres(CLOCK_REALTIME, resolution) != 0)
		die_errno("clock_getres");
}

static void
localtime_or_die(time_t value, struct tm *tm)
{
	if (localtime_r(&value, tm) == NULL)
		die("invalid time");
}

static int
parse_two_digits(const char *text, const char *what)
{
	if (!isdigit((unsigned char)text[0]) || !isdigit((unsigned char)text[1]))
		die("illegal time format");
	return ((text[0] - '0') * 10 + (text[1] - '0'));
}

static void
convert_tm_or_die(struct tm *tm, struct timespec *ts)
{
	time_t converted;

	tm->tm_yday = -1;
	converted = mktime(tm);
	if (tm->tm_yday == -1)
		die("nonexistent time");
	ts->tv_sec = converted;
	ts->tv_nsec = 0;
}

static void
parse_legacy_time(const char *text, const struct timespec *base, struct timespec *ts)
{
	struct tm tm;
	const char *dot;
	size_t digits_len;
	int second;

	localtime_or_die(base->tv_sec, &tm);
	tm.tm_isdst = -1;
	ts->tv_nsec = 0;

	dot = strchr(text, '.');
	if (dot != NULL) {
		if (strchr(dot + 1, '.') != NULL || strlen(dot + 1) != 2)
			die("illegal time format");
		second = parse_two_digits(dot + 1, "seconds");
		if (second > 61)
			die("illegal time format");
		tm.tm_sec = second;
		digits_len = (size_t)(dot - text);
	} else {
		tm.tm_sec = 0;
		digits_len = strlen(text);
	}

	if (digits_len == 0)
		die("illegal time format");
	for (size_t i = 0; i < digits_len; i++) {
		if (!isdigit((unsigned char)text[i]))
			die("illegal time format");
	}

	switch (digits_len) {
	case 12:
		tm.tm_year = parse_two_digits(text, "century") * 100 - TM_YEAR_BASE;
		tm.tm_year += parse_two_digits(text + 2, "year");
		tm.tm_mon = parse_two_digits(text + 4, "month") - 1;
		tm.tm_mday = parse_two_digits(text + 6, "day");
		tm.tm_hour = parse_two_digits(text + 8, "hour");
		tm.tm_min = parse_two_digits(text + 10, "minute");
		break;
	case 10: {
		int year;

		year = parse_two_digits(text, "year");
		tm.tm_year = year < 69 ? year + 100 : year;
		tm.tm_mon = parse_two_digits(text + 2, "month") - 1;
		tm.tm_mday = parse_two_digits(text + 4, "day");
		tm.tm_hour = parse_two_digits(text + 6, "hour");
		tm.tm_min = parse_two_digits(text + 8, "minute");
		break;
	}
	case 8:
		tm.tm_mon = parse_two_digits(text, "month") - 1;
		tm.tm_mday = parse_two_digits(text + 2, "day");
		tm.tm_hour = parse_two_digits(text + 4, "hour");
		tm.tm_min = parse_two_digits(text + 6, "minute");
		break;
	case 6:
		tm.tm_mday = parse_two_digits(text, "day");
		tm.tm_hour = parse_two_digits(text + 2, "hour");
		tm.tm_min = parse_two_digits(text + 4, "minute");
		break;
	case 4:
		tm.tm_hour = parse_two_digits(text, "hour");
		tm.tm_min = parse_two_digits(text + 2, "minute");
		break;
	case 2:
		tm.tm_min = parse_two_digits(text, "minute");
		break;
	default:
		die("illegal time format");
	}

	if (tm.tm_mon < 0 || tm.tm_mon > 11 || tm.tm_mday < 1 || tm.tm_mday > 31 ||
	    tm.tm_hour < 0 || tm.tm_hour > 23 || tm.tm_min < 0 || tm.tm_min > 59)
		die("illegal time format");

	convert_tm_or_die(&tm, ts);
}

static void
parse_formatted_time(const char *format, const char *text, const struct timespec *base,
    struct timespec *ts)
{
	struct tm tm;
	char *end;

	localtime_or_die(base->tv_sec, &tm);
	tm.tm_isdst = -1;
	end = strptime(text, format, &tm);
	if (end == NULL)
		die("failed conversion of '%s' using format '%s'", text, format);
	if (*end != '\0')
		die("input did not fully match format '%s': %s", format, text);

	convert_tm_or_die(&tm, ts);
}

static void
parse_time_operand(const struct options *options, const struct timespec *base,
    struct timespec *ts)
{
	if (options->input_format != NULL)
		parse_formatted_time(options->input_format, options->time_operand, base, ts);
	else
		parse_legacy_time(options->time_operand, base, ts);
}

static void
set_system_time(const struct timespec *ts)
{
	if (clock_settime(CLOCK_REALTIME, ts) != 0)
		die_errno("clock_settime");
}

static void
apply_variations(const struct options *options, struct tm *tm)
{
	const struct vary *failed;

	failed = vary_apply(options->vary_chain, tm);
	if (failed != NULL)
		die("cannot apply date adjustment: %s", failed->arg);
}

static bool
pending_is_nsec_prefix(const char *pending, size_t len)
{
	size_t i;

	if (len == 0 || pending[0] != '%')
		return (false);
	for (i = 1; i < len; i++) {
		if (pending[i] == '-' && i == 1)
			continue;
		if (!isdigit((unsigned char)pending[i]))
			return (false);
	}
	return (true);
}

static void
append_nsec_digits(struct strbuf *buf, const char *pending, size_t len, long nsec,
    long resolution)
{
	size_t i;
	int width;
	int zeroes;
	long value;
	char digits[32];
	int printed;

	width = 0;
	zeroes = 0;

	if (len == 2 && pending[1] == '-') {
		long number;

		for (width = 9, number = resolution; width > 0 && number > 0;
		    width--, number /= 10)
			;
	} else if (len > 1) {
		for (i = 1; i < len; i++) {
			if (width > (INT_MAX - (pending[i] - '0')) / 10)
				die("nanosecond width is too large");
			width = width * 10 + (pending[i] - '0');
		}
	}

	if (width == 0) {
		width = 9;
	} else if (width > 9) {
		zeroes = width - 9;
		width = 9;
	}

	value = nsec;
	for (i = 0; i < (size_t)(9 - width); i++)
		value /= 10;

	printed = snprintf(digits, sizeof(digits), "%0*ld", width, value);
	if (printed < 0 || (size_t)printed >= sizeof(digits))
		die("failed to render nanoseconds");

	strbuf_append_mem(buf, digits, (size_t)printed);
	while (zeroes-- > 0)
		strbuf_append_char(buf, '0');
}

static char *
expand_format_string(const char *format, long nsec, long resolution)
{
	struct strbuf result;
	char pending[64];
	size_t pending_len;
	size_t i;
	bool in_percent;

	strbuf_init(&result);
	pending_len = 0;
	in_percent = false;

	for (i = 0; format[i] != '\0';) {
		if (!in_percent) {
			if (format[i] == '%') {
				pending[0] = '%';
				pending_len = 1;
				in_percent = true;
			} else {
				strbuf_append_char(&result, format[i]);
			}
			i++;
			continue;
		}

		if (format[i] == 'N' && pending_is_nsec_prefix(pending, pending_len)) {
			append_nsec_digits(&result, pending, pending_len, nsec, resolution);
			in_percent = false;
			i++;
			continue;
		}
		if (format[i] == '+' && pending_len == 1) {
			strbuf_append_str(&result, DEFAULT_PLUS_FORMAT);
			in_percent = false;
			i++;
			continue;
		}
		if (format[i] == '%' ) {
			strbuf_append_mem(&result, pending, pending_len);
			strbuf_append_char(&result, '%');
			in_percent = false;
			i++;
			continue;
		}
		if (format[i] == '-' && pending_len == 1) {
			pending[pending_len++] = '-';
			i++;
			continue;
		}
		if (isdigit((unsigned char)format[i]) &&
		    pending_is_nsec_prefix(pending, pending_len) &&
		    !(pending_len == 2 && pending[1] == '-')) {
			if (pending_len >= sizeof(pending) - 1)
				die("format specifier too long");
			pending[pending_len++] = format[i];
			i++;
			continue;
		}

		strbuf_append_mem(&result, pending, pending_len);
		in_percent = false;
	}

	if (in_percent)
		strbuf_append_mem(&result, pending, pending_len);

	return (strbuf_finish(&result));
}

static char *
render_format(const char *format, const struct tm *tm, long nsec, long resolution)
{
	char *expanded;
	char *result;
	size_t size;
	size_t produced;

	if (format[0] == '\0') {
		result = strdup("");
		if (result == NULL)
			die_errno("strdup");
		return (result);
	}

	expanded = expand_format_string(format, nsec, resolution);
	size = 128;
	for (;;) {
		result = malloc(size);
		if (result == NULL)
			die_errno("malloc");

		produced = strftime(result, size, expanded, tm);
		if (produced != 0) {
			free(expanded);
			return (result);
		}

		free(result);
		if (size > 1 << 20)
			die("formatted output is too large");
		size *= 2;
	}
}

static char *
render_numeric_timezone(const struct tm *tm)
{
	char raw[16];
	struct strbuf buf;
	size_t len;

	len = strftime(raw, sizeof(raw), "%z", tm);
	if (len == 0)
		die("failed to render timezone offset");

	if (len != 5 || (raw[0] != '+' && raw[0] != '-')) {
		char *copy;

		copy = strdup(raw);
		if (copy == NULL)
			die_errno("strdup");
		return (copy);
	}

	strbuf_init(&buf);
	strbuf_append_mem(&buf, raw, 3);
	strbuf_append_char(&buf, ':');
	strbuf_append_mem(&buf, raw + 3, 2);
	return (strbuf_finish(&buf));
}

static char *
render_iso8601(const struct iso8601_fmt *selection, const struct tm *tm, long nsec,
    long resolution)
{
	char *timestamp;
	char *zone;
	struct strbuf result;

	timestamp = render_format(selection->format_string, tm, nsec, resolution);
	if (!selection->include_zone)
		return (timestamp);

	zone = render_numeric_timezone(tm);
	strbuf_init(&result);
	strbuf_append_str(&result, timestamp);
	strbuf_append_str(&result, zone);
	free(timestamp);
	free(zone);
	return (strbuf_finish(&result));
}

static void
print_line_and_exit(const char *text)
{
	if (printf("%s\n", text) < 0 || fflush(stdout) != 0)
		die_errno("stdout");
	free((void *)text);
	exit(EXIT_SUCCESS);
}

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: date [-jRu] [-I[date|hours|minutes|seconds|ns]] [-f input_fmt]",
	    "            "
	    "[ -z output_zone ] [-r filename|seconds] [-v[+|-]val[y|m|w|d|H|M|S]]",
	    "            "
	    "[[[[[[cc]yy]mm]dd]HH]MM[.SS] | new_date] [+output_fmt]");
	exit(EXIT_FAILURE);
}

static void
die(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

static void
die_errno(const char *fmt, ...)
{
	int saved_errno;
	va_list ap;

	saved_errno = errno;
	fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(saved_errno));
	exit(EXIT_FAILURE);
}
