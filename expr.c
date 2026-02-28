/*
 * SPDX-License-Identifier: Apache-2.0
 *
 *  Copyright (c) 2026 Project Tick
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * Linux/musl-native port of FreeBSD expr(1).
 * The original parser was expressed in yacc grammar form; this port keeps
 * the same operator precedence and value semantics but uses a local
 * recursive-descent parser so the standalone build does not depend on the
 * FreeBSD build system or generated sources.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR_EXIT 2

enum value_type {
	VALUE_INTEGER,
	VALUE_NUMERIC_STRING,
	VALUE_STRING,
};

struct value {
	enum value_type type;
	union {
		intmax_t integer;
		char *string;
	} data;
};

struct parser {
	const char *program;
	char **argv;
	int argc;
	int index;
	bool compat_mode;
};

static void usage(void) __attribute__((noreturn));
static void vfail(struct parser *parser, bool with_errno, const char *fmt,
    va_list ap) __attribute__((noreturn));
static void fail(struct parser *parser, const char *fmt, ...)
	__attribute__((format(printf, 2, 3), noreturn));
static void fail_errno(struct parser *parser, const char *fmt, ...)
	__attribute__((format(printf, 2, 3), noreturn));
static void *xmalloc(struct parser *parser, size_t size);
static char *xstrdup(struct parser *parser, const char *text);
static bool is_operator_token(const char *token);
static bool is_integer_literal(const char *text, bool compat_mode);
static struct value value_from_integer(intmax_t integer);
static struct value value_from_argument(struct parser *parser, const char *text);
static void value_dispose(struct value *value);
static struct value value_move(struct value *value);
static bool value_try_to_integer(struct value *value);
static void value_require_integer(struct parser *parser, struct value *value);
static void value_require_string(struct parser *parser, struct value *value);
static bool value_is_zero_or_null(struct value *value);
static int compare_values(struct parser *parser, struct value *left,
    struct value *right);
static bool add_overflow(intmax_t left, intmax_t right, intmax_t *result);
static bool sub_overflow(intmax_t left, intmax_t right, intmax_t *result);
static uintmax_t intmax_magnitude(intmax_t value);
static bool mul_overflow(intmax_t left, intmax_t right, intmax_t *result);
static struct value value_or(struct value *left, struct value *right);
static struct value value_and(struct value *left, struct value *right);
static struct value value_compare_eq(struct parser *parser, struct value *left,
    struct value *right);
static struct value value_compare_ne(struct parser *parser, struct value *left,
    struct value *right);
static struct value value_compare_lt(struct parser *parser, struct value *left,
    struct value *right);
static struct value value_compare_le(struct parser *parser, struct value *left,
    struct value *right);
static struct value value_compare_gt(struct parser *parser, struct value *left,
    struct value *right);
static struct value value_compare_ge(struct parser *parser, struct value *left,
    struct value *right);
static struct value value_add(struct parser *parser, struct value *left,
    struct value *right);
static struct value value_sub(struct parser *parser, struct value *left,
    struct value *right);
static struct value value_mul(struct parser *parser, struct value *left,
    struct value *right);
static struct value value_div(struct parser *parser, struct value *left,
    struct value *right);
static struct value value_rem(struct parser *parser, struct value *left,
    struct value *right);
static char *dup_substring(struct parser *parser, const char *text, regoff_t start,
    regoff_t end);
static struct value value_match(struct parser *parser, struct value *left,
    struct value *right);
static const char *peek_token(const struct parser *parser);
static bool match_token(struct parser *parser, const char *token);
static struct value parse_expression(struct parser *parser);
static struct value parse_or(struct parser *parser);
static struct value parse_and(struct parser *parser);
static struct value parse_compare(struct parser *parser);
static struct value parse_add(struct parser *parser);
static struct value parse_mul(struct parser *parser);
static struct value parse_match_expr(struct parser *parser);
static struct value parse_primary(struct parser *parser);
static void write_result(struct parser *parser, const struct value *value);

static void
usage(void)
{
	fprintf(stderr, "usage: expr [-e] [--] expression\n");
	exit(ERR_EXIT);
}

static void
vfail(struct parser *parser, bool with_errno, const char *fmt, va_list ap)
{
	int saved_errno;

	saved_errno = errno;
	fprintf(stderr, "%s: ", parser->program);
	vfprintf(stderr, fmt, ap);
	if (with_errno) {
		fprintf(stderr, ": %s",
		    saved_errno == 0 ? "I/O error" : strerror(saved_errno));
	}
	fputc('\n', stderr);
	exit(ERR_EXIT);
}

static void
fail(struct parser *parser, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfail(parser, false, fmt, ap);
	va_end(ap);
}

static void
fail_errno(struct parser *parser, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfail(parser, true, fmt, ap);
	va_end(ap);
}

static void *
xmalloc(struct parser *parser, size_t size)
{
	void *memory;

	if (size == 0) {
		size = 1;
	}
	memory = malloc(size);
	if (memory == NULL) {
		fail(parser, "malloc failed");
	}
	return (memory);
}

static char *
xstrdup(struct parser *parser, const char *text)
{
	size_t length;
	char *copy;

	length = strlen(text) + 1;
	copy = xmalloc(parser, length);
	memcpy(copy, text, length);
	return (copy);
}

static bool
is_operator_token(const char *token)
{
	static const char *const operators[] = {
		"|",
		"&",
		"=",
		">",
		"<",
		">=",
		"<=",
		"!=",
		"+",
		"-",
		"*",
		"/",
		"%",
		":",
		"(",
		")",
	};
	size_t i;

	for (i = 0; i < sizeof(operators) / sizeof(operators[0]); i++) {
		if (strcmp(token, operators[i]) == 0) {
			return (true);
		}
	}
	return (false);
}

static bool
is_integer_literal(const char *text, bool compat_mode)
{
	const unsigned char *cursor;

	cursor = (const unsigned char *)text;
	if (compat_mode) {
		if (*cursor == '\0') {
			return (true);
		}
		while (isspace(*cursor) != 0) {
			cursor++;
		}
	}
	if (*cursor == '-' || (compat_mode && *cursor == '+')) {
		cursor++;
	}
	if (*cursor == '\0') {
		return (false);
	}
	while (isdigit(*cursor) != 0) {
		cursor++;
	}
	return (*cursor == '\0');
}

static struct value
value_from_integer(intmax_t integer)
{
	struct value value;

	value.type = VALUE_INTEGER;
	value.data.integer = integer;
	return (value);
}

static struct value
value_from_argument(struct parser *parser, const char *text)
{
	struct value value;

	value.data.string = xstrdup(parser, text);
	value.type = is_integer_literal(text, parser->compat_mode) ?
	    VALUE_NUMERIC_STRING : VALUE_STRING;
	return (value);
}

static void
value_dispose(struct value *value)
{
	if (value->type == VALUE_NUMERIC_STRING || value->type == VALUE_STRING) {
		free(value->data.string);
	}
	value->type = VALUE_INTEGER;
	value->data.integer = 0;
}

static struct value
value_move(struct value *value)
{
	struct value moved;

	moved = *value;
	value->type = VALUE_INTEGER;
	value->data.integer = 0;
	return (moved);
}

static bool
value_try_to_integer(struct value *value)
{
	intmax_t parsed;
	char *end;

	if (value->type == VALUE_INTEGER) {
		return (true);
	}
	if (value->type != VALUE_NUMERIC_STRING) {
		return (false);
	}

	errno = 0;
	end = NULL;
	parsed = strtoimax(value->data.string, &end, 10);
	if (errno == ERANGE || end == NULL || *end != '\0') {
		return (false);
	}

	free(value->data.string);
	value->type = VALUE_INTEGER;
	value->data.integer = parsed;
	return (true);
}

static void
value_require_integer(struct parser *parser, struct value *value)
{
	if (value->type == VALUE_STRING) {
		fail(parser, "not a decimal number: '%s'", value->data.string);
	}
	if (!value_try_to_integer(value)) {
		fail(parser, "operand too large: '%s'", value->data.string);
	}
}

static void
value_require_string(struct parser *parser, struct value *value)
{
	int length;
	size_t size;
	char *buffer;

	if (value->type != VALUE_INTEGER) {
		return;
	}

	length = snprintf(NULL, 0, "%jd", value->data.integer);
	if (length < 0) {
		fail(parser, "failed to format integer result");
	}
	size = (size_t)length + 1;
	buffer = xmalloc(parser, size);
	if (snprintf(buffer, size, "%jd", value->data.integer) != length) {
		free(buffer);
		fail(parser, "failed to format integer result");
	}
	value->type = VALUE_STRING;
	value->data.string = buffer;
}

static bool
value_is_zero_or_null(struct value *value)
{
	if (value->type == VALUE_INTEGER) {
		return (value->data.integer == 0);
	}
	if (value->data.string[0] == '\0') {
		return (true);
	}
	return (value_try_to_integer(value) && value->data.integer == 0);
}

static int
compare_values(struct parser *parser, struct value *left, struct value *right)
{
	int result;

	if (left->type == VALUE_STRING || right->type == VALUE_STRING) {
		value_require_string(parser, left);
		value_require_string(parser, right);
		result = strcoll(left->data.string, right->data.string);
	} else {
		value_require_integer(parser, left);
		value_require_integer(parser, right);
		if (left->data.integer > right->data.integer) {
			result = 1;
		} else if (left->data.integer < right->data.integer) {
			result = -1;
		} else {
			result = 0;
		}
	}

	value_dispose(left);
	value_dispose(right);
	return (result);
}

static bool
add_overflow(intmax_t left, intmax_t right, intmax_t *result)
{
	if (right > 0 && left > INTMAX_MAX - right) {
		return (true);
	}
	if (right < 0 && left < INTMAX_MIN - right) {
		return (true);
	}
	*result = left + right;
	return (false);
}

static bool
sub_overflow(intmax_t left, intmax_t right, intmax_t *result)
{
	if (right > 0 && left < INTMAX_MIN + right) {
		return (true);
	}
	if (right < 0 && left > INTMAX_MAX + right) {
		return (true);
	}
	*result = left - right;
	return (false);
}

static uintmax_t
intmax_magnitude(intmax_t value)
{
	if (value >= 0) {
		return ((uintmax_t)value);
	}
	return ((uintmax_t)(-(value + 1)) + 1U);
}

static bool
mul_overflow(intmax_t left, intmax_t right, intmax_t *result)
{
	uintmax_t left_abs;
	uintmax_t right_abs;
	uintmax_t limit;
	uintmax_t product;
	bool negative;

	if (left == 0 || right == 0) {
		*result = 0;
		return (false);
	}

	left_abs = intmax_magnitude(left);
	right_abs = intmax_magnitude(right);
	negative = ((left < 0) != (right < 0));
	limit = negative ? ((uintmax_t)INTMAX_MAX + 1U) : (uintmax_t)INTMAX_MAX;

	if (right_abs > limit / left_abs) {
		return (true);
	}

	product = left_abs * right_abs;
	if (!negative) {
		*result = (intmax_t)product;
		return (false);
	}
	if (product == (uintmax_t)INTMAX_MAX + 1U) {
		*result = INTMAX_MIN;
		return (false);
	}
	*result = -(intmax_t)product;
	return (false);
}

static struct value
value_or(struct value *left, struct value *right)
{
	if (!value_is_zero_or_null(left)) {
		value_dispose(right);
		return (value_move(left));
	}
	value_dispose(left);
	if (!value_is_zero_or_null(right)) {
		return (value_move(right));
	}
	value_dispose(right);
	return (value_from_integer(0));
}

static struct value
value_and(struct value *left, struct value *right)
{
	if (value_is_zero_or_null(left) || value_is_zero_or_null(right)) {
		value_dispose(left);
		value_dispose(right);
		return (value_from_integer(0));
	}
	value_dispose(right);
	return (value_move(left));
}

static struct value
value_compare_eq(struct parser *parser, struct value *left, struct value *right)
{
	return (value_from_integer(compare_values(parser, left, right) == 0));
}

static struct value
value_compare_ne(struct parser *parser, struct value *left, struct value *right)
{
	return (value_from_integer(compare_values(parser, left, right) != 0));
}

static struct value
value_compare_lt(struct parser *parser, struct value *left, struct value *right)
{
	return (value_from_integer(compare_values(parser, left, right) < 0));
}

static struct value
value_compare_le(struct parser *parser, struct value *left, struct value *right)
{
	return (value_from_integer(compare_values(parser, left, right) <= 0));
}

static struct value
value_compare_gt(struct parser *parser, struct value *left, struct value *right)
{
	return (value_from_integer(compare_values(parser, left, right) > 0));
}

static struct value
value_compare_ge(struct parser *parser, struct value *left, struct value *right)
{
	return (value_from_integer(compare_values(parser, left, right) >= 0));
}

static struct value
value_add(struct parser *parser, struct value *left, struct value *right)
{
	intmax_t result;

	value_require_integer(parser, left);
	value_require_integer(parser, right);
	if (add_overflow(left->data.integer, right->data.integer, &result)) {
		fail(parser, "overflow");
	}
	value_dispose(left);
	value_dispose(right);
	return (value_from_integer(result));
}

static struct value
value_sub(struct parser *parser, struct value *left, struct value *right)
{
	intmax_t result;

	value_require_integer(parser, left);
	value_require_integer(parser, right);
	if (sub_overflow(left->data.integer, right->data.integer, &result)) {
		fail(parser, "overflow");
	}
	value_dispose(left);
	value_dispose(right);
	return (value_from_integer(result));
}

static struct value
value_mul(struct parser *parser, struct value *left, struct value *right)
{
	intmax_t result;

	value_require_integer(parser, left);
	value_require_integer(parser, right);
	if (mul_overflow(left->data.integer, right->data.integer, &result)) {
		fail(parser, "overflow");
	}
	value_dispose(left);
	value_dispose(right);
	return (value_from_integer(result));
}

static struct value
value_div(struct parser *parser, struct value *left, struct value *right)
{
	intmax_t result;

	value_require_integer(parser, left);
	value_require_integer(parser, right);
	if (right->data.integer == 0) {
		fail(parser, "division by zero");
	}
	if (left->data.integer == INTMAX_MIN && right->data.integer == -1) {
		fail(parser, "overflow");
	}
	result = left->data.integer / right->data.integer;
	value_dispose(left);
	value_dispose(right);
	return (value_from_integer(result));
}

static struct value
value_rem(struct parser *parser, struct value *left, struct value *right)
{
	intmax_t result;

	value_require_integer(parser, left);
	value_require_integer(parser, right);
	if (right->data.integer == 0) {
		fail(parser, "division by zero");
	}
	result = left->data.integer % right->data.integer;
	value_dispose(left);
	value_dispose(right);
	return (value_from_integer(result));
}

static char *
dup_substring(struct parser *parser, const char *text, regoff_t start, regoff_t end)
{
	size_t length;
	char *result;

	if (start < 0 || end < start) {
		fail(parser, "regular expression returned an invalid capture range");
	}
	length = (size_t)(end - start);
	result = xmalloc(parser, length + 1);
	memcpy(result, text + start, length);
	result[length] = '\0';
	return (result);
}

static struct value
value_match(struct parser *parser, struct value *left, struct value *right)
{
	regex_t regex;
	regmatch_t matches[2];
	struct value result;
	size_t error_length;
	char *error_text;
	int regcomp_result;
	int regexec_result;

	value_require_string(parser, left);
	value_require_string(parser, right);

	regcomp_result = regcomp(&regex, right->data.string, 0);
	if (regcomp_result != 0) {
		error_length = regerror(regcomp_result, &regex, NULL, 0);
		error_text = xmalloc(parser, error_length);
		(void)regerror(regcomp_result, &regex, error_text, error_length);
		fail(parser, "regular expression error: %s", error_text);
	}

	regexec_result = regexec(&regex, left->data.string,
	    (size_t)(sizeof(matches) / sizeof(matches[0])), matches, 0);
	if (regexec_result == 0 && matches[0].rm_so == 0) {
		if (matches[1].rm_so >= 0) {
			result.type = VALUE_STRING;
			result.data.string = dup_substring(parser, left->data.string,
			    matches[1].rm_so, matches[1].rm_eo);
		} else {
			result = value_from_integer(matches[0].rm_eo);
		}
	} else if (regexec_result == REG_NOMATCH ||
	    (regexec_result == 0 && matches[0].rm_so != 0)) {
		if (regex.re_nsub == 0) {
			result = value_from_integer(0);
		} else {
			result.type = VALUE_STRING;
			result.data.string = xstrdup(parser, "");
		}
	} else {
		error_length = regerror(regexec_result, &regex, NULL, 0);
		error_text = xmalloc(parser, error_length);
		(void)regerror(regexec_result, &regex, error_text, error_length);
		regfree(&regex);
		fail(parser, "regular expression execution failed: %s", error_text);
	}

	regfree(&regex);
	value_dispose(left);
	value_dispose(right);
	return (result);
}

static const char *
peek_token(const struct parser *parser)
{
	if (parser->index >= parser->argc) {
		return (NULL);
	}
	return (parser->argv[parser->index]);
}

static bool
match_token(struct parser *parser, const char *token)
{
	const char *current;

	current = peek_token(parser);
	if (current == NULL || strcmp(current, token) != 0) {
		return (false);
	}
	parser->index++;
	return (true);
}

static struct value
parse_expression(struct parser *parser)
{
	return (parse_or(parser));
}

static struct value
parse_or(struct parser *parser)
{
	struct value left;
	struct value right;

	left = parse_and(parser);
	while (match_token(parser, "|")) {
		right = parse_and(parser);
		left = value_or(&left, &right);
	}
	return (left);
}

static struct value
parse_and(struct parser *parser)
{
	struct value left;
	struct value right;

	left = parse_compare(parser);
	while (match_token(parser, "&")) {
		right = parse_compare(parser);
		left = value_and(&left, &right);
	}
	return (left);
}

static struct value
parse_compare(struct parser *parser)
{
	struct value left;
	struct value right;

	left = parse_add(parser);
	for (;;) {
		if (match_token(parser, "=")) {
			right = parse_add(parser);
			left = value_compare_eq(parser, &left, &right);
		} else if (match_token(parser, "!=")) {
			right = parse_add(parser);
			left = value_compare_ne(parser, &left, &right);
		} else if (match_token(parser, ">=")) {
			right = parse_add(parser);
			left = value_compare_ge(parser, &left, &right);
		} else if (match_token(parser, "<=")) {
			right = parse_add(parser);
			left = value_compare_le(parser, &left, &right);
		} else if (match_token(parser, ">")) {
			right = parse_add(parser);
			left = value_compare_gt(parser, &left, &right);
		} else if (match_token(parser, "<")) {
			right = parse_add(parser);
			left = value_compare_lt(parser, &left, &right);
		} else {
			break;
		}
	}
	return (left);
}

static struct value
parse_add(struct parser *parser)
{
	struct value left;
	struct value right;

	left = parse_mul(parser);
	for (;;) {
		if (match_token(parser, "+")) {
			right = parse_mul(parser);
			left = value_add(parser, &left, &right);
		} else if (match_token(parser, "-")) {
			right = parse_mul(parser);
			left = value_sub(parser, &left, &right);
		} else {
			break;
		}
	}
	return (left);
}

static struct value
parse_mul(struct parser *parser)
{
	struct value left;
	struct value right;

	left = parse_match_expr(parser);
	for (;;) {
		if (match_token(parser, "*")) {
			right = parse_match_expr(parser);
			left = value_mul(parser, &left, &right);
		} else if (match_token(parser, "/")) {
			right = parse_match_expr(parser);
			left = value_div(parser, &left, &right);
		} else if (match_token(parser, "%")) {
			right = parse_match_expr(parser);
			left = value_rem(parser, &left, &right);
		} else {
			break;
		}
	}
	return (left);
}

static struct value
parse_match_expr(struct parser *parser)
{
	struct value left;
	struct value right;

	left = parse_primary(parser);
	while (match_token(parser, ":")) {
		right = parse_primary(parser);
		left = value_match(parser, &left, &right);
	}
	return (left);
}

static struct value
parse_primary(struct parser *parser)
{
	const char *token;
	struct value value;

	token = peek_token(parser);
	if (token == NULL) {
		fail(parser, "syntax error");
	}
	if (match_token(parser, "(")) {
		value = parse_expression(parser);
		if (!match_token(parser, ")")) {
			value_dispose(&value);
			fail(parser, "syntax error");
		}
		return (value);
	}
	if (is_operator_token(token)) {
		fail(parser, "syntax error");
	}
	parser->index++;
	return (value_from_argument(parser, token));
}

static void
write_result(struct parser *parser, const struct value *value)
{
	int rc;

	if (value->type == VALUE_INTEGER) {
		rc = fprintf(stdout, "%jd\n", value->data.integer);
	} else {
		rc = fprintf(stdout, "%s\n", value->data.string);
	}
	if (rc < 0 || fflush(stdout) == EOF) {
		fail_errno(parser, "write failed");
	}
}

int
main(int argc, char *argv[])
{
	struct parser parser;
	struct value result;
	const char *arg;
	int i;

	(void)setlocale(LC_ALL, "");

	parser.program = argv[0] == NULL ? "expr" : argv[0];
	if (strrchr(parser.program, '/') != NULL) {
		parser.program = strrchr(parser.program, '/') + 1;
	}
	parser.compat_mode = false;
	parser.index = 0;
	parser.argv = NULL;
	parser.argc = 0;

	i = 1;
	while (i < argc) {
		arg = argv[i];
		if (strcmp(arg, "--") == 0) {
			i++;
			break;
		}
		if (strcmp(arg, "-e") == 0) {
			parser.compat_mode = true;
			i++;
			continue;
		}
		if (arg[0] == '-' && arg[1] != '\0') {
			usage();
		}
		break;
	}

	parser.argv = argv + i;
	parser.argc = argc - i;
	if (parser.argc == 0) {
		usage();
	}

	result = parse_expression(&parser);
	if (peek_token(&parser) != NULL) {
		value_dispose(&result);
		fail(&parser, "syntax error");
	}

	write_result(&parser, &result);
	i = value_is_zero_or_null(&result) ? 1 : 0;
	value_dispose(&result);
	return (i);
}
