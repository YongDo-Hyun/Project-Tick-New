/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
 * All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <ctype.h>
#include <err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "vary.h"

struct trans {
	int64_t value;
	const char *name;
};

static const struct trans trans_mon[] = {
	{ 1, "january" }, { 2, "february" }, { 3, "march" }, { 4, "april" },
	{ 5, "may" }, { 6, "june" }, { 7, "july" }, { 8, "august" },
	{ 9, "september" }, { 10, "october" }, { 11, "november" },
	{ 12, "december" }, { -1, NULL }
};

static const struct trans trans_wday[] = {
	{ 0, "sunday" }, { 1, "monday" }, { 2, "tuesday" }, { 3, "wednesday" },
	{ 4, "thursday" }, { 5, "friday" }, { 6, "saturday" }, { -1, NULL }
};

static int mdays[12] = { 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static int adjyear(struct tm *tm, char type, int64_t value, bool normalize);
static int adjmon(struct tm *tm, char type, int64_t value, bool is_text,
    bool normalize);
static int adjday(struct tm *tm, char type, int64_t value, bool normalize);
static int adjwday(struct tm *tm, char type, int64_t value, bool is_text,
    bool normalize);
static int adjhour(struct tm *tm, char type, int64_t value, bool normalize);
static int adjmin(struct tm *tm, char type, int64_t value, bool normalize);
static int adjsec(struct tm *tm, char type, int64_t value, bool normalize);

static bool normalize_tm(struct tm *tm, char type);
static int translate_token(const struct trans *table, const char *arg);
static bool parse_decimal_component(const char *arg, size_t len, int64_t *value);
static int daysinmonth(const struct tm *tm);

static bool
normalize_tm(struct tm *tm, char type)
{
	time_t converted;

	tm->tm_yday = -1;
	while ((converted = mktime(tm)) == (time_t)-1 &&
	    tm->tm_year > 68 && tm->tm_year < 138) {
		if (!adjhour(tm, type == '-' ? '-' : '+', 1, false))
			return (false);
	}

	return (tm->tm_yday != -1);
}

static int
translate_token(const struct trans *table, const char *arg)
{
	size_t i;

	for (i = 0; table[i].value != -1; i++) {
		if (strncasecmp(table[i].name, arg, 3) == 0 ||
		    strcasecmp(table[i].name, arg) == 0)
			return ((int)table[i].value);
	}

	return (-1);
}

struct vary *
vary_append(struct vary *chain, const char *arg)
{
	struct vary *node;
	struct vary **nextp;

	if (chain == NULL) {
		nextp = &chain;
	} else {
		nextp = &chain->next;
		while (*nextp != NULL)
			nextp = &(*nextp)->next;
	}

	node = malloc(sizeof(*node));
	if (node == NULL)
		err(1, "malloc");

	node->arg = arg;
	node->next = NULL;
	*nextp = node;
	return (chain);
}

static bool
parse_decimal_component(const char *arg, size_t len, int64_t *value)
{
	size_t i;
	int64_t current;
	int digit;

	if (len == 0)
		return (false);

	current = 0;
	for (i = 0; i < len; i++) {
		if (!isdigit((unsigned char)arg[i]))
			return (false);
		digit = arg[i] - '0';
		if (current > (INT64_MAX - digit) / 10)
			return (false);
		current = current * 10 + digit;
	}

	*value = current;
	return (true);
}

static int
daysinmonth(const struct tm *tm)
{
	int year;

	year = tm->tm_year + 1900;
	if (tm->tm_mon == 1) {
		if (year % 400 == 0)
			return (29);
		if (year % 100 == 0)
			return (28);
		if (year % 4 == 0)
			return (29);
		return (28);
	}
	if (tm->tm_mon >= 0 && tm->tm_mon < 12)
		return (mdays[tm->tm_mon]);
	return (0);
}

static int
adjyear(struct tm *tm, char type, int64_t value, bool normalize)
{
	switch (type) {
	case '+':
		tm->tm_year += value;
		break;
	case '-':
		tm->tm_year -= value;
		break;
	default:
		tm->tm_year = value;
		if (tm->tm_year < 69)
			tm->tm_year += 100;
		else if (tm->tm_year > 1900)
			tm->tm_year -= 1900;
		break;
	}

	return (!normalize || normalize_tm(tm, type));
}

static int
adjmon(struct tm *tm, char type, int64_t value, bool is_text, bool normalize)
{
	int last_month_days;

	if (value < 0)
		return (0);

	switch (type) {
	case '+':
		if (is_text) {
			if (value <= tm->tm_mon)
				value += 11 - tm->tm_mon;
			else
				value -= tm->tm_mon + 1;
		}
		if (value != 0) {
			if (!adjyear(tm, '+', (tm->tm_mon + value) / 12, false))
				return (0);
			value %= 12;
			tm->tm_mon += value;
			if (tm->tm_mon > 11)
				tm->tm_mon -= 12;
		}
		break;
	case '-':
		if (is_text) {
			if (value - 1 > tm->tm_mon)
				value = 13 - value + tm->tm_mon;
			else
				value = tm->tm_mon - value + 1;
		}
		if (value != 0) {
			if (!adjyear(tm, '-', value / 12, false))
				return (0);
			value %= 12;
			if (value > tm->tm_mon) {
				if (!adjyear(tm, '-', 1, false))
					return (0);
				value -= 12;
			}
			tm->tm_mon -= value;
		}
		break;
	default:
		if (value < 1 || value > 12)
			return (0);
		tm->tm_mon = (int)value - 1;
		break;
	}

	last_month_days = daysinmonth(tm);
	if (tm->tm_mday > last_month_days)
		tm->tm_mday = last_month_days;

	return (!normalize || normalize_tm(tm, type));
}

static int
adjday(struct tm *tm, char type, int64_t value, bool normalize)
{
	int last_month_days;

	switch (type) {
	case '+':
		while (value != 0) {
			last_month_days = daysinmonth(tm);
			if (value > last_month_days - tm->tm_mday) {
				value -= last_month_days - tm->tm_mday + 1;
				tm->tm_mday = 1;
				if (!adjmon(tm, '+', 1, false, false))
					return (0);
			} else {
				tm->tm_mday += value;
				value = 0;
			}
		}
		break;
	case '-':
		while (value != 0) {
			if (value >= tm->tm_mday) {
				value -= tm->tm_mday;
				tm->tm_mday = 1;
				if (!adjmon(tm, '-', 1, false, false))
					return (0);
				tm->tm_mday = daysinmonth(tm);
			} else {
				tm->tm_mday -= value;
				value = 0;
			}
		}
		break;
	default:
		if (value > 0 && value <= daysinmonth(tm))
			tm->tm_mday = (int)value;
		else
			return (0);
		break;
	}

	return (!normalize || normalize_tm(tm, type));
}

static int
adjwday(struct tm *tm, char type, int64_t value, bool is_text, bool normalize)
{
	if (value < 0)
		return (0);

	switch (type) {
	case '+':
		if (is_text) {
			if (value < tm->tm_wday)
				value = 7 - tm->tm_wday + value;
			else
				value -= tm->tm_wday;
		} else {
			value *= 7;
		}
		return (!value || adjday(tm, '+', value, normalize));
	case '-':
		if (is_text) {
			if (value > tm->tm_wday)
				value = 7 - value + tm->tm_wday;
			else
				value = tm->tm_wday - value;
		} else {
			value *= 7;
		}
		return (!value || adjday(tm, '-', value, normalize));
	default:
		if (value < tm->tm_wday)
			return (adjday(tm, '-', tm->tm_wday - value, normalize));
		if (value > 6)
			return (0);
		if (value > tm->tm_wday)
			return (adjday(tm, '+', value - tm->tm_wday, normalize));
		break;
	}

	return (1);
}

static int
adjhour(struct tm *tm, char type, int64_t value, bool normalize)
{
	if (value < 0)
		return (0);

	switch (type) {
	case '+':
		if (value != 0) {
			int days;

			days = (int)((tm->tm_hour + value) / 24);
			value %= 24;
			tm->tm_hour += value;
			tm->tm_hour %= 24;
			if (!adjday(tm, '+', days, false))
				return (0);
		}
		break;
	case '-':
		if (value != 0) {
			int days;

			days = (int)(value / 24);
			value %= 24;
			if (value > tm->tm_hour) {
				days++;
				value -= 24;
			}
			tm->tm_hour -= value;
			if (!adjday(tm, '-', days, false))
				return (0);
		}
		break;
	default:
		if (value > 23)
			return (0);
		tm->tm_hour = (int)value;
		break;
	}

	return (!normalize || normalize_tm(tm, type));
}

static int
adjmin(struct tm *tm, char type, int64_t value, bool normalize)
{
	if (value < 0)
		return (0);

	switch (type) {
	case '+':
		if (value != 0) {
			if (!adjhour(tm, '+', (tm->tm_min + value) / 60, false))
				return (0);
			value %= 60;
			tm->tm_min += value;
			if (tm->tm_min > 59)
				tm->tm_min -= 60;
		}
		break;
	case '-':
		if (value != 0) {
			if (!adjhour(tm, '-', value / 60, false))
				return (0);
			value %= 60;
			if (value > tm->tm_min) {
				if (!adjhour(tm, '-', 1, false))
					return (0);
				value -= 60;
			}
			tm->tm_min -= value;
		}
		break;
	default:
		if (value > 59)
			return (0);
		tm->tm_min = (int)value;
		break;
	}

	return (!normalize || normalize_tm(tm, type));
}

static int
adjsec(struct tm *tm, char type, int64_t value, bool normalize)
{
	if (value < 0)
		return (0);

	switch (type) {
	case '+':
		if (value != 0) {
			if (!adjmin(tm, '+', (tm->tm_sec + value) / 60, false))
				return (0);
			value %= 60;
			tm->tm_sec += value;
			if (tm->tm_sec > 59)
				tm->tm_sec -= 60;
		}
		break;
	case '-':
		if (value != 0) {
			if (!adjmin(tm, '-', value / 60, false))
				return (0);
			value %= 60;
			if (value > tm->tm_sec) {
				if (!adjmin(tm, '-', 1, false))
					return (0);
				value -= 60;
			}
			tm->tm_sec -= value;
		}
		break;
	default:
		if (value > 59)
			return (0);
		tm->tm_sec = (int)value;
		break;
	}

	return (!normalize || normalize_tm(tm, type));
}

const struct vary *
vary_apply(const struct vary *chain, struct tm *tm)
{
	const struct vary *node;

	for (node = chain; node != NULL; node = node->next) {
		const char *arg;
		char type;
		char unit;
		int64_t value;
		size_t len;
		int translated;

		type = node->arg[0];
		arg = node->arg;
		if (type == '+' || type == '-')
			arg++;
		else
			type = '\0';

		len = strlen(arg);
		if (len < 2)
			return (node);

		if (type == '\0')
			tm->tm_isdst = -1;

		if (!parse_decimal_component(arg, len - 1, &value)) {
			translated = translate_token(trans_wday, arg);
			if (translated != -1) {
				if (!adjwday(tm, type, translated, true, true))
					return (node);
				continue;
			}

			translated = translate_token(trans_mon, arg);
			if (translated != -1) {
				if (!adjmon(tm, type, translated, true, true))
					return (node);
				continue;
			}

			return (node);
		}

		unit = arg[len - 1];
		switch (unit) {
		case 'S':
			if (!adjsec(tm, type, value, true))
				return (node);
			break;
		case 'M':
			if (!adjmin(tm, type, value, true))
				return (node);
			break;
		case 'H':
			if (!adjhour(tm, type, value, true))
				return (node);
			break;
		case 'd':
			tm->tm_isdst = -1;
			if (!adjday(tm, type, value, true))
				return (node);
			break;
		case 'w':
			tm->tm_isdst = -1;
			if (!adjwday(tm, type, value, false, true))
				return (node);
			break;
		case 'm':
			tm->tm_isdst = -1;
			if (!adjmon(tm, type, value, false, true))
				return (node);
			break;
		case 'y':
			tm->tm_isdst = -1;
			if (!adjyear(tm, type, value, true))
				return (node);
			break;
		default:
			return (node);
		}
	}

	return (NULL);
}

void
vary_destroy(struct vary *chain)
{
	struct vary *next;

	while (chain != NULL) {
		next = chain->next;
		free(chain);
		chain = next;
	}
}
