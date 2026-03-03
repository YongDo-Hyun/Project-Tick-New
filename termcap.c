#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "termcap.h"

struct capability {
	const char *name;
	const char *value;
};

struct flag_capability {
	const char *name;
	int value;
};

struct num_capability {
	const char *name;
	int value;
};

static const struct capability ansi_caps[] = {
	{ "al", "\033[L" },
	{ "bl", "\a" },
	{ "cd", "\033[J" },
	{ "ce", "\033[K" },
	{ "ch", "\033[%i%dG" },
	{ "cl", "\033[H\033[2J" },
	{ "dc", "\033[P" },
	{ "dl", "\033[M" },
	{ "dm", "" },
	{ "ed", "" },
	{ "ei", "" },
	{ "fs", "" },
	{ "ho", "\033[H" },
	{ "ic", "\033[@" },
	{ "im", "" },
	{ "ip", "" },
	{ "kd", "\033[B" },
	{ "kl", "\033[D" },
	{ "kr", "\033[C" },
	{ "ku", "\033[A" },
	{ "md", "\033[1m" },
	{ "me", "\033[0m" },
	{ "nd", "\033[C" },
	{ "se", "\033[0m" },
	{ "so", "\033[7m" },
	{ "ts", "" },
	{ "up", "\033[A" },
	{ "us", "\033[4m" },
	{ "ue", "\033[0m" },
	{ "vb", "\a" },
	{ "DC", "\033[%dP" },
	{ "DO", "\033[%dB" },
	{ "IC", "\033[%d@" },
	{ "LE", "\033[%dD" },
	{ "RI", "\033[%dC" },
	{ "UP", "\033[%dA" },
	{ "kh", "\033[H" },
	{ "@7", "\033[F" },
	{ "kD", "\033[3~" },
	{ NULL, NULL }
};

static const struct flag_capability flag_caps[] = {
	{ "am", 1 },
	{ "km", 1 },
	{ "pt", 1 },
	{ "xt", 0 },
	{ "xn", 0 },
	{ "MT", 0 },
	{ NULL, 0 }
};

static char tgoto_buf[64];
static int term_columns = 80;
static int term_lines = 24;

static const char *
find_capability(const char *id)
{
	size_t i;

	for (i = 0; ansi_caps[i].name != NULL; i++) {
		if (strcmp(ansi_caps[i].name, id) == 0)
			return ansi_caps[i].value;
	}
	return NULL;
}

static void
update_terminal_size(void)
{
	struct winsize ws;
	const char *env;
	char *end;
	long value;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
		if (ws.ws_col > 0)
			term_columns = ws.ws_col;
		if (ws.ws_row > 0)
			term_lines = ws.ws_row;
	}

	env = getenv("COLUMNS");
	if (env != NULL && *env != '\0') {
		errno = 0;
		value = strtol(env, &end, 10);
		if (errno == 0 && *end == '\0' && value > 0 && value <= INT_MAX)
			term_columns = (int)value;
	}

	env = getenv("LINES");
	if (env != NULL && *env != '\0') {
		errno = 0;
		value = strtol(env, &end, 10);
		if (errno == 0 && *end == '\0' && value > 0 && value <= INT_MAX)
			term_lines = (int)value;
	}
}

int
tgetent(char *bp, const char *name)
{
	(void)bp;
	(void)name;
	update_terminal_size();
	return 1;
}

int
tgetflag(const char *id)
{
	size_t i;

	for (i = 0; flag_caps[i].name != NULL; i++) {
		if (strcmp(flag_caps[i].name, id) == 0)
			return flag_caps[i].value;
	}
	return 0;
}

int
tgetnum(const char *id)
{
	if (strcmp(id, "co") == 0)
		return term_columns;
	if (strcmp(id, "li") == 0)
		return term_lines;
	return -1;
}

char *
tgetstr(const char *id, char **area)
{
	const char *value;
	size_t len;
	char *dst;

	value = find_capability(id);
	if (value == NULL)
		return NULL;
	if (area == NULL)
		return (char *)value;

	len = strlen(value) + 1;
	dst = *area;
	memcpy(dst, value, len);
	*area += len;
	return dst;
}

int
tputs(const char *str, int affcnt, int (*putc_fn)(int))
{
	const unsigned char *p;

	(void)affcnt;
	if (str == NULL || putc_fn == NULL)
		return -1;
	for (p = (const unsigned char *)str; *p != '\0'; p++) {
		if (putc_fn(*p) == EOF)
			return -1;
	}
	return 0;
}

static int
next_param(const int params[2], int *index)
{
	int value;

	if (*index >= 2)
		return 0;
	value = params[*index];
	(*index)++;
	if (value < 0)
		value = -value;
	return value;
}

char *
tgoto(const char *cap, int col, int row)
{
	const unsigned char *src;
	char *dst;
	int params[2];
	int param_index;

	if (cap == NULL)
		return NULL;

	params[0] = col;
	params[1] = row;
	param_index = 0;
	dst = tgoto_buf;
	src = (const unsigned char *)cap;

	while (*src != '\0' && (size_t)(dst - tgoto_buf) < sizeof(tgoto_buf) - 1) {
		if (*src != '%') {
			*dst++ = (char)*src++;
			continue;
		}

		src++;
		switch (*src) {
		case '%':
			*dst++ = '%';
			src++;
			break;
		case 'd': {
			int value;
			int written;

			value = next_param(params, &param_index);
			written = snprintf(dst,
			    sizeof(tgoto_buf) - (size_t)(dst - tgoto_buf),
			    "%d", value);
			if (written < 0)
				return NULL;
			dst += written;
			src++;
			break;
		}
		case '2':
		case '3': {
			int width;
			int value;
			int written;

			width = *src - '0';
			value = next_param(params, &param_index);
			written = snprintf(dst,
			    sizeof(tgoto_buf) - (size_t)(dst - tgoto_buf),
			    "%0*d", width, value);
			if (written < 0)
				return NULL;
			dst += written;
			src++;
			break;
		}
		case '.': {
			int value;

			value = next_param(params, &param_index);
			if (value == 0)
				value = ' ';
			*dst++ = (char)value;
			src++;
			break;
		}
		case '+': {
			int value;

			src++;
			value = next_param(params, &param_index);
			*dst++ = (char)(value + *src++);
			break;
		}
		case 'i':
			params[0]++;
			params[1]++;
			src++;
			break;
		case 'r': {
			int tmp;

			tmp = params[0];
			params[0] = params[1];
			params[1] = tmp;
			src++;
			break;
		}
		default:
			*dst++ = '%';
			if (*src != '\0')
				*dst++ = (char)*src++;
			break;
		}
	}

	*dst = '\0';
	return tgoto_buf;
}
