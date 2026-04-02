/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 *
 * Copyright (c) 2026
 *  Project Tick. All rights reserved.
 *
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND
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

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sysexits.h>
#include <unistd.h>

#define MOUNTINFO_PATH "/proc/self/mountinfo"
#define DEFAULT_BLOCK_SIZE 512ULL
#define MAX_BLOCK_SIZE (1ULL << 30)

enum display_mode {
	DISPLAY_BLOCKS = 0,
	DISPLAY_HUMAN_1024,
	DISPLAY_HUMAN_1000,
};

struct string_list {
	char **items;
	size_t len;
	bool invert;
	bool present;
};

struct options {
	bool aflag;
	bool cflag;
	bool iflag;
	bool kflag_seen;
	bool lflag;
	bool thousands;
	bool Tflag;
	enum display_mode mode;
	bool block_size_override;
	uint64_t block_size;
	struct string_list type_filter;
};

struct mount_entry {
	char *fstype;
	char *options;
	char *source;
	char *target;
	dev_t dev;
	bool is_remote;
};

struct mount_table {
	struct mount_entry *entries;
	size_t len;
	size_t cap;
};

struct row {
	const struct mount_entry *mount;
	bool is_total;
	uint64_t total_bytes;
	uint64_t used_bytes;
	uint64_t avail_bytes;
	uint64_t total_inodes;
	uint64_t free_inodes;
};

struct row_list {
	struct row *items;
	size_t len;
	size_t cap;
};

struct column_widths {
	int avail;
	int capacity;
	int fstype;
	int ifree;
	int iused;
	int mount_source;
	int size;
	int used;
};

static void append_mount(struct mount_table *table, struct mount_entry entry);
static void append_row(struct row_list *rows, struct row row);
static bool contains_token(const char *csv, const char *token);
static uint64_t divide_saturated(uint64_t value, uint64_t divisor);
static void format_block_header(char *buf, size_t buflen, uint64_t block_size);
static void format_human(char *buf, size_t buflen, uint64_t value, bool si_units,
    bool bytes);
static void format_integer(char *buf, size_t buflen, uint64_t value,
    const char *separator);
static void free_mount_table(struct mount_table *table);
static void free_row_list(struct row_list *rows);
static void free_string_list(struct string_list *list);
static const struct mount_entry *find_mount_for_path(const struct mount_table *table,
    const char *path);
static const struct mount_entry *find_mount_for_source(const struct mount_table *table,
    const char *source);
static int load_mounts(struct mount_table *table);
static int parse_block_size_env(void);
static int parse_mount_line(char *line, struct mount_entry *entry);
static int parse_options(int argc, char *argv[], struct options *options);
static int parse_type_filter(const char *arg, struct string_list *list);
static bool path_is_within_mount(const char *path, const char *mountpoint);
static int populate_all_rows(const struct options *options,
    const struct mount_table *table, struct row_list *rows);
static int populate_operand_rows(const struct options *options,
    const struct mount_table *table, char *const *operands, int operand_count,
    struct row_list *rows);
static void print_rows(const struct options *options, const struct row_list *rows);
static int resolve_operand(const struct mount_table *table, const char *operand,
    const struct mount_entry **mount, char **canonical_path);
static bool row_selected(const struct options *options,
    const struct mount_entry *mount);
static int row_statvfs(const struct mount_entry *mount, struct row *row);
static int scan_mountinfo(FILE *stream, struct mount_table *table);
static void set_default_options(struct options *options);
static char *strdup_or_die(const char *value);
static bool string_list_contains(const struct string_list *list, const char *value);
static bool is_remote_mount(const char *fstype, const char *source,
    const char *options);
static char *unescape_mount_field(const char *value);
static void usage(void);
static void update_widths(const struct options *options, struct column_widths *widths,
    const struct row *row);
static uint64_t clamp_product(uint64_t lhs, uint64_t rhs);
static uint64_t clamp_sum(uint64_t lhs, uint64_t rhs);
static uint64_t row_fragment_size(const struct statvfs *stats);
static char *split_next_field(char **cursor, char delimiter);

static void
append_mount(struct mount_table *table, struct mount_entry entry)
{
	struct mount_entry *new_entries;
	size_t new_cap;

	if (table->len < table->cap) {
		table->entries[table->len++] = entry;
		return;
	}

	new_cap = table->cap == 0 ? 32 : table->cap * 2;
	if (new_cap < table->cap) {
		fprintf(stderr, "df: mount table too large\n");
		exit(EX_OSERR);
	}
	new_entries = realloc(table->entries, new_cap * sizeof(*table->entries));
	if (new_entries == NULL) {
		fprintf(stderr, "df: realloc failed\n");
		exit(EX_OSERR);
	}
	table->entries = new_entries;
	table->cap = new_cap;
	table->entries[table->len++] = entry;
}

static void
append_row(struct row_list *rows, struct row row)
{
	struct row *new_items;
	size_t new_cap;

	if (rows->len < rows->cap) {
		rows->items[rows->len++] = row;
		return;
	}

	new_cap = rows->cap == 0 ? 16 : rows->cap * 2;
	if (new_cap < rows->cap) {
		fprintf(stderr, "df: row table too large\n");
		exit(EX_OSERR);
	}
	new_items = realloc(rows->items, new_cap * sizeof(*rows->items));
	if (new_items == NULL) {
		fprintf(stderr, "df: realloc failed\n");
		exit(EX_OSERR);
	}
	rows->items = new_items;
	rows->cap = new_cap;
	rows->items[rows->len++] = row;
}

static bool
contains_token(const char *csv, const char *token)
{
	const char *cursor;
	size_t token_len;

	if (csv == NULL || token == NULL)
		return false;

	token_len = strlen(token);
	for (cursor = csv; *cursor != '\0'; ) {
		const char *end;
		size_t len;

		end = strchr(cursor, ',');
		if (end == NULL)
			end = cursor + strlen(cursor);
		len = (size_t)(end - cursor);
		if (len == token_len && strncmp(cursor, token, len) == 0)
			return true;
		if (*end == '\0')
			break;
		cursor = end + 1;
	}

	return false;
}

static uint64_t
divide_saturated(uint64_t value, uint64_t divisor)
{
	if (divisor == 0)
		return 0;
	return value / divisor;
}

static void
format_block_header(char *buf, size_t buflen, uint64_t block_size)
{
	const char *suffix;
	uint64_t units;

	if (block_size == DEFAULT_BLOCK_SIZE) {
		snprintf(buf, buflen, "512-blocks");
		return;
	}

	if (block_size % (1ULL << 30) == 0) {
		suffix = "G";
		units = block_size / (1ULL << 30);
	} else if (block_size % (1ULL << 20) == 0) {
		suffix = "M";
		units = block_size / (1ULL << 20);
	} else if (block_size % 1024ULL == 0) {
		suffix = "K";
		units = block_size / 1024ULL;
	} else {
		suffix = "";
		units = block_size;
	}

	snprintf(buf, buflen, "%ju%s-blocks", (uintmax_t)units, suffix);
}

static void
format_human(char *buf, size_t buflen, uint64_t value, bool si_units, bool bytes)
{
	static const char *const byte_units[] = { "B", "K", "M", "G", "T", "P", "E" };
	static const char *const count_units[] = { "", "K", "M", "G", "T", "P", "E" };
	const char *const *units;
	double scaled;
	unsigned base;
	size_t unit_index;

	base = si_units ? 1000U : 1024U;
	units = bytes ? byte_units : count_units;
	scaled = (double)value;
	unit_index = 0;

	while (scaled >= (double)base && unit_index < 6) {
		scaled /= (double)base;
		unit_index++;
	}

	if (unit_index == 0) {
		snprintf(buf, buflen, "%ju%s", (uintmax_t)value, units[unit_index]);
		return;
	}

	if (scaled < 10.0)
		snprintf(buf, buflen, "%.1f%s", scaled, units[unit_index]);
	else
		snprintf(buf, buflen, "%.0f%s", scaled, units[unit_index]);
}

static void
format_integer(char *buf, size_t buflen, uint64_t value, const char *separator)
{
	char digits[64];
	char reversed[96];
	size_t digit_count;
	size_t out_len;
	size_t i;
	size_t sep_len;

	sep_len = separator == NULL ? 0 : strlen(separator);
	snprintf(digits, sizeof(digits), "%ju", (uintmax_t)value);
	if (sep_len == 0) {
		snprintf(buf, buflen, "%s", digits);
		return;
	}

	digit_count = strlen(digits);
	out_len = 0;
	for (i = 0; i < digit_count; i++) {
		if (i != 0 && i % 3 == 0) {
			size_t j;

			for (j = 0; j < sep_len; j++) {
				if (out_len + 1 >= sizeof(reversed))
					break;
				reversed[out_len++] = separator[sep_len - j - 1];
			}
		}
		if (out_len + 1 >= sizeof(reversed))
			break;
		reversed[out_len++] = digits[digit_count - i - 1];
	}
	for (i = 0; i < out_len && i + 1 < buflen; i++)
		buf[i] = reversed[out_len - i - 1];
	buf[i] = '\0';
}

static void
free_mount_table(struct mount_table *table)
{
	size_t i;

	for (i = 0; i < table->len; i++) {
		free(table->entries[i].fstype);
		free(table->entries[i].options);
		free(table->entries[i].source);
		free(table->entries[i].target);
	}
	free(table->entries);
	table->entries = NULL;
	table->len = 0;
	table->cap = 0;
}

static void
free_row_list(struct row_list *rows)
{
	free(rows->items);
	rows->items = NULL;
	rows->len = 0;
	rows->cap = 0;
}

static void
free_string_list(struct string_list *list)
{
	size_t i;

	for (i = 0; i < list->len; i++)
		free(list->items[i]);
	free(list->items);
	list->items = NULL;
	list->len = 0;
	list->invert = false;
	list->present = false;
}

static const struct mount_entry *
find_mount_for_path(const struct mount_table *table, const char *path)
{
	const struct mount_entry *best;
	size_t best_len;
	size_t i;

	best = NULL;
	best_len = 0;
	for (i = 0; i < table->len; i++) {
		size_t mount_len;

		if (!path_is_within_mount(path, table->entries[i].target))
			continue;
		mount_len = strlen(table->entries[i].target);
		if (mount_len >= best_len) {
			best = &table->entries[i];
			best_len = mount_len;
		}
	}

	return best;
}

static const struct mount_entry *
find_mount_for_source(const struct mount_table *table, const char *source)
{
	size_t i;

	for (i = 0; i < table->len; i++) {
		if (strcmp(table->entries[i].source, source) == 0)
			return &table->entries[i];
	}

	return NULL;
}

static int
load_mounts(struct mount_table *table)
{
	FILE *stream;
	int rc;

	stream = fopen(MOUNTINFO_PATH, "re");
	if (stream == NULL) {
		fprintf(stderr, "df: %s: %s\n", MOUNTINFO_PATH, strerror(errno));
		return -1;
	}

	rc = scan_mountinfo(stream, table);
	fclose(stream);
	return rc;
}

static int
parse_block_size_env(void)
{
	char *endptr;
	const char *value;
	uint64_t multiplier;
	uint64_t numeric;
	unsigned long long parsed;

	value = getenv("BLOCKSIZE");
	if (value == NULL || *value == '\0')
		return (int)DEFAULT_BLOCK_SIZE;

	errno = 0;
	parsed = strtoull(value, &endptr, 10);
	if (errno != 0 || endptr == value)
		return (int)DEFAULT_BLOCK_SIZE;

	multiplier = 1;
	if (*endptr != '\0') {
		if (endptr[1] != '\0')
			return (int)DEFAULT_BLOCK_SIZE;
		switch (*endptr) {
		case 'k':
		case 'K':
			multiplier = 1024ULL;
			break;
		case 'm':
		case 'M':
			multiplier = 1ULL << 20;
			break;
		case 'g':
		case 'G':
			multiplier = 1ULL << 30;
			break;
		default:
			return (int)DEFAULT_BLOCK_SIZE;
		}
	}

	numeric = (uint64_t)parsed;
	if (numeric != 0 && multiplier > UINT64_MAX / numeric)
		numeric = MAX_BLOCK_SIZE;
	else
		numeric *= multiplier;

	if (numeric < DEFAULT_BLOCK_SIZE)
		numeric = DEFAULT_BLOCK_SIZE;
	if (numeric > MAX_BLOCK_SIZE)
		numeric = MAX_BLOCK_SIZE;

	return (int)numeric;
}

static int
parse_mount_line(char *line, struct mount_entry *entry)
{
	char *cursor;
	char *dash;
	char *fields[6];
	char *right_fields[3];
	char *mount_options;
	char *super_options;
	char *combined;
	unsigned long major_id;
	unsigned long minor_id;
	size_t i;

	memset(entry, 0, sizeof(*entry));
	dash = strstr(line, " - ");
	if (dash == NULL)
		return -1;
	*dash = '\0';
	cursor = line;
	for (i = 0; i < 6; i++) {
		fields[i] = split_next_field(&cursor, ' ');
		if (fields[i] == NULL)
			return -1;
	}
	cursor = dash + 3;
	for (i = 0; i < 3; i++) {
		right_fields[i] = split_next_field(&cursor, ' ');
		if (right_fields[i] == NULL)
			return -1;
	}

	if (sscanf(fields[2], "%lu:%lu", &major_id, &minor_id) != 2)
		return -1;

	entry->target = unescape_mount_field(fields[4]);
	entry->fstype = strdup_or_die(right_fields[0]);
	entry->source = unescape_mount_field(right_fields[1]);
	mount_options = fields[5];
	super_options = right_fields[2];
	combined = malloc(strlen(mount_options) + strlen(super_options) + 2);
	if (combined == NULL) {
		fprintf(stderr, "df: malloc failed\n");
		exit(EX_OSERR);
	}
	snprintf(combined, strlen(mount_options) + strlen(super_options) + 2,
	    "%s,%s", mount_options, super_options);
	entry->options = combined;
	entry->dev = makedev(major_id, minor_id);
	entry->is_remote = is_remote_mount(entry->fstype, entry->source,
	    entry->options);
	return 0;
}

static int
parse_options(int argc, char *argv[], struct options *options)
{
	static const struct option long_options[] = {
		{ "si", no_argument, NULL, 'H' },
		{ NULL, 0, NULL, 0 },
	};
	int ch;

	while ((ch = getopt_long(argc, argv, "+abcgHhiklmnPt:T,", long_options,
	    NULL)) != -1) {
		switch (ch) {
		case 'a':
			options->aflag = true;
			break;
		case 'b':
		case 'P':
			if (!options->kflag_seen) {
				options->mode = DISPLAY_BLOCKS;
				options->block_size_override = true;
				options->block_size = DEFAULT_BLOCK_SIZE;
			}
			break;
		case 'c':
			options->cflag = true;
			break;
		case 'g':
			options->mode = DISPLAY_BLOCKS;
			options->block_size_override = true;
			options->block_size = 1ULL << 30;
			break;
		case 'H':
			options->mode = DISPLAY_HUMAN_1000;
			options->block_size_override = false;
			break;
		case 'h':
			options->mode = DISPLAY_HUMAN_1024;
			options->block_size_override = false;
			break;
		case 'i':
			options->iflag = true;
			break;
		case 'k':
			options->kflag_seen = true;
			options->mode = DISPLAY_BLOCKS;
			options->block_size_override = true;
			options->block_size = 1024;
			break;
		case 'l':
			options->lflag = true;
			break;
		case 'm':
			options->mode = DISPLAY_BLOCKS;
			options->block_size_override = true;
			options->block_size = 1ULL << 20;
			break;
		case 'n':
			fprintf(stderr, "df: option -n is not supported on Linux\n");
			return -1;
		case 't':
			if (options->type_filter.present) {
				fprintf(stderr, "df: only one -t option may be specified\n");
				return -1;
			}
			if (parse_type_filter(optarg, &options->type_filter) != 0)
				return -1;
			break;
		case 'T':
			options->Tflag = true;
			break;
		case ',':
			options->thousands = true;
			break;
		default:
			usage();
		}
	}

	if (options->mode == DISPLAY_BLOCKS && !options->block_size_override)
		options->block_size = (uint64_t)parse_block_size_env();

	return 0;
}

static int
parse_type_filter(const char *arg, struct string_list *list)
{
	char *copy;
	char *token;
	char *cursor;

	if (arg == NULL) {
		fprintf(stderr, "df: missing filesystem type list\n");
		return -1;
	}

	copy = strdup_or_die(arg);
	cursor = copy;
	list->invert = false;
	if (strncmp(cursor, "no", 2) == 0) {
		list->invert = true;
		cursor += 2;
	}
	if (*cursor == '\0') {
		fprintf(stderr, "df: empty filesystem type list: %s\n", arg);
		free(copy);
		return -1;
	}

	while ((token = split_next_field(&cursor, ',')) != NULL) {
		char **items;

		if (*token == '\0') {
			fprintf(stderr, "df: empty filesystem type in list: %s\n", arg);
			free(copy);
			return -1;
		}
		items = realloc(list->items, (list->len + 1) * sizeof(*list->items));
		if (items == NULL) {
			fprintf(stderr, "df: realloc failed\n");
			free(copy);
			exit(EX_OSERR);
		}
		list->items = items;
		list->items[list->len++] = strdup_or_die(token);
	}
	free(copy);
	list->present = true;
	return 0;
}

static bool
path_is_within_mount(const char *path, const char *mountpoint)
{
	size_t mount_len;

	if (strcmp(mountpoint, "/") == 0)
		return path[0] == '/';

	mount_len = strlen(mountpoint);
	if (strncmp(path, mountpoint, mount_len) != 0)
		return false;
	if (path[mount_len] == '\0' || path[mount_len] == '/')
		return true;
	return false;
}

static int
populate_all_rows(const struct options *options, const struct mount_table *table,
    struct row_list *rows)
{
	int rv;
	size_t i;

	rv = 0;
	for (i = 0; i < table->len; i++) {
		struct row row;

		if (!row_selected(options, &table->entries[i]))
			continue;
		if (row_statvfs(&table->entries[i], &row) != 0) {
			fprintf(stderr, "df: %s: %s\n", table->entries[i].target,
			    strerror(errno));
			rv = 1;
			continue;
		}
		append_row(rows, row);
	}

	return rv;
}

static int
populate_operand_rows(const struct options *options, const struct mount_table *table,
    char *const *operands, int operand_count, struct row_list *rows)
{
	int rv;
	int i;

	rv = 0;
	for (i = 0; i < operand_count; i++) {
		char *canonical_path;
		const struct mount_entry *mount;
		struct row row;

		canonical_path = NULL;
		if (resolve_operand(table, operands[i], &mount, &canonical_path) != 0) {
			rv = 1;
			free(canonical_path);
			continue;
		}
		if (!row_selected(options, mount)) {
			fprintf(stderr,
			    "df: %s: filtered out by current filesystem selection\n",
			    operands[i]);
			rv = 1;
			free(canonical_path);
			continue;
		}
		if (row_statvfs(mount, &row) != 0) {
			fprintf(stderr, "df: %s: %s\n", mount->target, strerror(errno));
			rv = 1;
			free(canonical_path);
			continue;
		}
		append_row(rows, row);
		free(canonical_path);
	}

	return rv;
}

static void
print_rows(const struct options *options, const struct row_list *rows)
{
	struct column_widths widths;
	char header[32];
	char size_buf[64];
	char used_buf[64];
	char avail_buf[64];
	char iused_buf[64];
	char ifree_buf[64];
	char inode_pct[16];
	char percent_buf[16];
	const char *separator;
	size_t i;

	if (rows->len == 0)
		return;

	memset(&widths, 0, sizeof(widths));
	for (i = 0; i < rows->len; i++)
		update_widths(options, &widths, &rows->items[i]);

	if (options->mode == DISPLAY_BLOCKS)
		format_block_header(header, sizeof(header), options->block_size);
	else
		snprintf(header, sizeof(header), "Size");
	if (widths.mount_source < (int)strlen("Filesystem"))
		widths.mount_source = (int)strlen("Filesystem");
	if (options->Tflag && widths.fstype < (int)strlen("Type"))
		widths.fstype = (int)strlen("Type");
	if (widths.size < (int)strlen(header))
		widths.size = (int)strlen(header);
	if (widths.used < (int)strlen("Used"))
		widths.used = (int)strlen("Used");
	if (widths.avail < (int)strlen("Avail"))
		widths.avail = (int)strlen("Avail");
	if (widths.capacity < (int)strlen("Capacity"))
		widths.capacity = (int)strlen("Capacity");
	if (options->iflag) {
		if (widths.iused < (int)strlen("iused"))
			widths.iused = (int)strlen("iused");
		if (widths.ifree < (int)strlen("ifree"))
			widths.ifree = (int)strlen("ifree");
	}

	printf("%-*s", widths.mount_source, "Filesystem");
	if (options->Tflag)
		printf("  %-*s", widths.fstype, "Type");
	printf(" %*s %*s %*s %*s", widths.size, header, widths.used, "Used",
	    widths.avail, "Avail", widths.capacity, "Capacity");
	if (options->iflag)
		printf(" %*s %*s %6s", widths.iused, "iused", widths.ifree, "ifree",
		    "%iused");
	printf("  Mounted on\n");

	separator = options->thousands ? localeconv()->thousands_sep : "";
	if (separator == NULL)
		separator = "";

	for (i = 0; i < rows->len; i++) {
		uint64_t used_inodes;
		const char *mount_source;
		const char *mount_target;
		const char *fstype;
		double percent;

		mount_source = rows->items[i].is_total ? "total" : rows->items[i].mount->source;
		mount_target = rows->items[i].is_total ? "" : rows->items[i].mount->target;
		fstype = rows->items[i].is_total ? "" : rows->items[i].mount->fstype;

		if (options->mode == DISPLAY_BLOCKS) {
			format_integer(size_buf, sizeof(size_buf),
			    divide_saturated(rows->items[i].total_bytes, options->block_size),
			    separator);
			format_integer(used_buf, sizeof(used_buf),
			    divide_saturated(rows->items[i].used_bytes, options->block_size),
			    separator);
			format_integer(avail_buf, sizeof(avail_buf),
			    divide_saturated(rows->items[i].avail_bytes, options->block_size),
			    separator);
		} else {
			format_human(size_buf, sizeof(size_buf), rows->items[i].total_bytes,
			    options->mode == DISPLAY_HUMAN_1000, true);
			format_human(used_buf, sizeof(used_buf), rows->items[i].used_bytes,
			    options->mode == DISPLAY_HUMAN_1000, true);
			format_human(avail_buf, sizeof(avail_buf), rows->items[i].avail_bytes,
			    options->mode == DISPLAY_HUMAN_1000, true);
		}

		if (rows->items[i].used_bytes + rows->items[i].avail_bytes == 0) {
			snprintf(percent_buf, sizeof(percent_buf), "100%%");
		} else {
			percent = ((double)rows->items[i].used_bytes * 100.0) /
			    (double)(rows->items[i].used_bytes + rows->items[i].avail_bytes);
			snprintf(percent_buf, sizeof(percent_buf), "%.0f%%", percent);
		}

		printf("%-*s", widths.mount_source, mount_source);
		if (options->Tflag)
			printf("  %-*s", widths.fstype, fstype);
		printf(" %*s %*s %*s %*s", widths.size, size_buf, widths.used, used_buf,
		    widths.avail, avail_buf, widths.capacity, percent_buf);

		if (options->iflag) {
			used_inodes = rows->items[i].total_inodes >= rows->items[i].free_inodes ?
			    rows->items[i].total_inodes - rows->items[i].free_inodes : 0;
			if (options->mode == DISPLAY_BLOCKS) {
				format_integer(iused_buf, sizeof(iused_buf), used_inodes,
				    separator);
				format_integer(ifree_buf, sizeof(ifree_buf),
				    rows->items[i].free_inodes, separator);
			} else {
				format_human(iused_buf, sizeof(iused_buf), used_inodes, true,
				    false);
				format_human(ifree_buf, sizeof(ifree_buf),
				    rows->items[i].free_inodes, true, false);
			}
			if (rows->items[i].total_inodes == 0)
				snprintf(inode_pct, sizeof(inode_pct), "-");
			else
				snprintf(inode_pct, sizeof(inode_pct), "%.0f%%",
				    ((double)used_inodes * 100.0) /
				    (double)rows->items[i].total_inodes);
			printf(" %*s %*s %6s", widths.iused, iused_buf, widths.ifree,
			    ifree_buf, inode_pct);
		}

		if (mount_target[0] != '\0')
			printf("  %s", mount_target);
		printf("\n");
	}
}

static int
resolve_operand(const struct mount_table *table, const char *operand,
    const struct mount_entry **mount, char **canonical_path)
{
	struct stat st;
	char *resolved;

	if (stat(operand, &st) != 0) {
		*mount = find_mount_for_source(table, operand);
		if (*mount != NULL)
			return 0;
		fprintf(stderr, "df: %s: %s\n", operand, strerror(errno));
		return -1;
	}

	if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
		*mount = find_mount_for_source(table, operand);
		if (*mount == NULL) {
			fprintf(stderr, "df: %s: not mounted\n", operand);
			return -1;
		}
		return 0;
	}

	resolved = realpath(operand, NULL);
	if (resolved == NULL) {
		fprintf(stderr, "df: %s: %s\n", operand, strerror(errno));
		return -1;
	}

	*mount = find_mount_for_path(table, resolved);
	if (*mount == NULL) {
		fprintf(stderr, "df: %s: no mount entry found\n", operand);
		free(resolved);
		return -1;
	}

	*canonical_path = resolved;
	return 0;
}

static bool
row_selected(const struct options *options, const struct mount_entry *mount)
{
	bool type_match;

	if (!options->type_filter.present)
		return !options->lflag || !mount->is_remote;

	type_match = string_list_contains(&options->type_filter, mount->fstype);
	if (options->type_filter.invert)
		return (!options->lflag || !mount->is_remote) && !type_match;
	if (options->lflag)
		return !mount->is_remote || type_match;
	return type_match;
}

static int
row_statvfs(const struct mount_entry *mount, struct row *row)
{
	struct statvfs stats;
	uint64_t fragment_size;
	uint64_t blocks;
	uint64_t bfree;
	uint64_t bavail;

	if (statvfs(mount->target, &stats) != 0)
		return -1;

	fragment_size = row_fragment_size(&stats);
	blocks = (uint64_t)stats.f_blocks;
	bfree = (uint64_t)stats.f_bfree;
	bavail = (uint64_t)stats.f_bavail;
	if (blocks < bfree)
		bfree = blocks;

	row->mount = mount;
	row->is_total = false;
	row->total_bytes = clamp_product(blocks, fragment_size);
	row->used_bytes = clamp_product(blocks - bfree, fragment_size);
	row->avail_bytes = clamp_product(bavail, fragment_size);
	row->total_inodes = (uint64_t)stats.f_files;
	row->free_inodes = (uint64_t)stats.f_ffree;
	return 0;
}

static int
scan_mountinfo(FILE *stream, struct mount_table *table)
{
	char *line;
	size_t linecap;
	ssize_t linelen;

	line = NULL;
	linecap = 0;
	while ((linelen = getline(&line, &linecap, stream)) != -1) {
		struct mount_entry entry;

		if (linelen > 0 && line[linelen - 1] == '\n')
			line[linelen - 1] = '\0';
		if (parse_mount_line(line, &entry) != 0) {
			fprintf(stderr, "df: could not parse %s\n", MOUNTINFO_PATH);
			free(line);
			return -1;
		}
		append_mount(table, entry);
	}
	free(line);
	if (ferror(stream)) {
		fprintf(stderr, "df: %s: %s\n", MOUNTINFO_PATH, strerror(errno));
		return -1;
	}
	return 0;
}

static void
set_default_options(struct options *options)
{
	memset(options, 0, sizeof(*options));
	options->mode = DISPLAY_BLOCKS;
	options->block_size = DEFAULT_BLOCK_SIZE;
}

static char *
strdup_or_die(const char *value)
{
	char *copy;

	copy = strdup(value);
	if (copy == NULL) {
		fprintf(stderr, "df: strdup failed\n");
		exit(EX_OSERR);
	}
	return copy;
}

static bool
string_list_contains(const struct string_list *list, const char *value)
{
	size_t i;

	for (i = 0; i < list->len; i++) {
		if (strcmp(list->items[i], value) == 0)
			return true;
	}

	return false;
}

static bool
is_remote_mount(const char *fstype, const char *source, const char *options)
{
	static const char *const remote_types[] = {
		"9p",
		"acfs",
		"afs",
		"ceph",
		"cifs",
		"coda",
		"davfs",
		"fuse.glusterfs",
		"fuse.sshfs",
		"gfs",
		"gfs2",
		"glusterfs",
		"lustre",
		"ncp",
		"ncpfs",
		"nfs",
		"nfs4",
		"ocfs2",
		"smb3",
		"smbfs",
		"sshfs",
	};
	size_t i;

	if (contains_token(options, "_netdev"))
		return true;
	for (i = 0; i < sizeof(remote_types) / sizeof(remote_types[0]); i++) {
		if (strcmp(fstype, remote_types[i]) == 0)
			return true;
	}
	if (strncmp(source, "//", 2) == 0)
		return true;
	if (source[0] != '/' && strstr(source, ":/") != NULL)
		return true;
	return false;
}

static char *
unescape_mount_field(const char *value)
{
	char *out;
	size_t in_len;
	size_t i;
	size_t j;

	in_len = strlen(value);
	out = malloc(in_len + 1);
	if (out == NULL) {
		fprintf(stderr, "df: malloc failed\n");
		exit(EX_OSERR);
	}

	for (i = 0, j = 0; i < in_len; i++) {
		if (value[i] == '\\' && i + 3 < in_len &&
		    isdigit((unsigned char)value[i + 1]) &&
		    isdigit((unsigned char)value[i + 2]) &&
		    isdigit((unsigned char)value[i + 3])) {
			unsigned digit;

			digit = (unsigned)(value[i + 1] - '0') * 64U +
			    (unsigned)(value[i + 2] - '0') * 8U +
			    (unsigned)(value[i + 3] - '0');
			out[j++] = (char)digit;
			i += 3;
			continue;
		}
		out[j++] = value[i];
	}
	out[j] = '\0';
	return out;
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: df [-b | -g | -H | -h | -k | -m | -P] [-acilT] [-t type] [-,]\n"
	    "          [file | filesystem ...]\n");
	exit(EX_USAGE);
}

static void
update_widths(const struct options *options, struct column_widths *widths,
    const struct row *row)
{
	char buf[64];
	char human_buf[32];
	uint64_t used_inodes;
	int len;

	len = (int)strlen(row->is_total ? "total" : row->mount->source);
	if (len > widths->mount_source)
		widths->mount_source = len;
	if (options->Tflag) {
		len = row->is_total ? 0 : (int)strlen(row->mount->fstype);
		if (len > widths->fstype)
			widths->fstype = len;
	}

	if (options->mode == DISPLAY_BLOCKS) {
		format_integer(buf, sizeof(buf),
		    divide_saturated(row->total_bytes, options->block_size),
		    options->thousands ? localeconv()->thousands_sep : "");
		len = (int)strlen(buf);
		if (len > widths->size)
			widths->size = len;
		format_integer(buf, sizeof(buf),
		    divide_saturated(row->used_bytes, options->block_size),
		    options->thousands ? localeconv()->thousands_sep : "");
		len = (int)strlen(buf);
		if (len > widths->used)
			widths->used = len;
		format_integer(buf, sizeof(buf),
		    divide_saturated(row->avail_bytes, options->block_size),
		    options->thousands ? localeconv()->thousands_sep : "");
		len = (int)strlen(buf);
		if (len > widths->avail)
			widths->avail = len;
	} else {
		format_human(human_buf, sizeof(human_buf), row->total_bytes,
		    options->mode == DISPLAY_HUMAN_1000, true);
		len = (int)strlen(human_buf);
		if (len > widths->size)
			widths->size = len;
		format_human(human_buf, sizeof(human_buf), row->used_bytes,
		    options->mode == DISPLAY_HUMAN_1000, true);
		len = (int)strlen(human_buf);
		if (len > widths->used)
			widths->used = len;
		format_human(human_buf, sizeof(human_buf), row->avail_bytes,
		    options->mode == DISPLAY_HUMAN_1000, true);
		len = (int)strlen(human_buf);
		if (len > widths->avail)
			widths->avail = len;
	}

	if (row->used_bytes + row->avail_bytes == 0)
		len = (int)strlen("100%");
	else
		len = 4;
	if (len > widths->capacity)
		widths->capacity = len;

	if (!options->iflag)
		return;

	used_inodes = row->total_inodes >= row->free_inodes ?
	    row->total_inodes - row->free_inodes : 0;
	if (options->mode == DISPLAY_BLOCKS) {
		format_integer(buf, sizeof(buf), used_inodes,
		    options->thousands ? localeconv()->thousands_sep : "");
		len = (int)strlen(buf);
		if (len > widths->iused)
			widths->iused = len;
		format_integer(buf, sizeof(buf), row->free_inodes,
		    options->thousands ? localeconv()->thousands_sep : "");
		len = (int)strlen(buf);
		if (len > widths->ifree)
			widths->ifree = len;
	} else {
		format_human(human_buf, sizeof(human_buf), used_inodes, true, false);
		len = (int)strlen(human_buf);
		if (len > widths->iused)
			widths->iused = len;
		format_human(human_buf, sizeof(human_buf), row->free_inodes, true,
		    false);
		len = (int)strlen(human_buf);
		if (len > widths->ifree)
			widths->ifree = len;
	}
}

static uint64_t
clamp_product(uint64_t lhs, uint64_t rhs)
{
	if (lhs == 0 || rhs == 0)
		return 0;
	if (lhs > UINT64_MAX / rhs)
		return UINT64_MAX;
	return lhs * rhs;
}

static uint64_t
clamp_sum(uint64_t lhs, uint64_t rhs)
{
	if (UINT64_MAX - lhs < rhs)
		return UINT64_MAX;
	return lhs + rhs;
}

static uint64_t
row_fragment_size(const struct statvfs *stats)
{
	if (stats->f_frsize != 0)
		return (uint64_t)stats->f_frsize;
	if (stats->f_bsize != 0)
		return (uint64_t)stats->f_bsize;
	return DEFAULT_BLOCK_SIZE;
}

static char *
split_next_field(char **cursor, char delimiter)
{
	char *field;
	char *end;

	if (cursor == NULL || *cursor == NULL)
		return NULL;

	while (**cursor == delimiter)
		(*cursor)++;
	if (**cursor == '\0')
		return NULL;

	field = *cursor;
	end = strchr(field, delimiter);
	if (end == NULL) {
		*cursor = field + strlen(field);
		return field;
	}

	*end = '\0';
	*cursor = end + 1;
	return field;
}

int
main(int argc, char *argv[])
{
	struct mount_table table;
	struct options options;
	struct row total_row;
	struct row_list rows;
	int rv;
	size_t i;

	(void)setlocale(LC_ALL, "");
	memset(&table, 0, sizeof(table));
	memset(&rows, 0, sizeof(rows));
	memset(&total_row, 0, sizeof(total_row));

	set_default_options(&options);
	if (parse_options(argc, argv, &options) != 0) {
		free_string_list(&options.type_filter);
		return EXIT_FAILURE;
	}

	if (load_mounts(&table) != 0) {
		free_string_list(&options.type_filter);
		return EXIT_FAILURE;
	}

	rv = 0;
	if (optind == argc)
		rv = populate_all_rows(&options, &table, &rows);
	else
		rv = populate_operand_rows(&options, &table, argv + optind,
		    argc - optind, &rows);

	if (options.cflag && rows.len > 0) {
		total_row.is_total = true;
		for (i = 0; i < rows.len; i++) {
			total_row.total_bytes = clamp_sum(total_row.total_bytes,
			    rows.items[i].total_bytes);
			total_row.used_bytes = clamp_sum(total_row.used_bytes,
			    rows.items[i].used_bytes);
			total_row.avail_bytes = clamp_sum(total_row.avail_bytes,
			    rows.items[i].avail_bytes);
			total_row.total_inodes = clamp_sum(total_row.total_inodes,
			    rows.items[i].total_inodes);
			total_row.free_inodes = clamp_sum(total_row.free_inodes,
			    rows.items[i].free_inodes);
		}
		append_row(&rows, total_row);
	}

	print_rows(&options, &rows);

	free_row_list(&rows);
	free_mount_table(&table);
	free_string_list(&options.type_filter);
	return rv == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
