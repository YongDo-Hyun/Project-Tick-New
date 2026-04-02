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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ls.h"
#include "extern.h"

static void compute_display_info(const struct context *ctx,
    const struct entry_list *list, struct display_info *info);
static void print_single(const struct context *ctx,
    const struct entry_list *list, const struct display_info *info);
static void print_columns(const struct context *ctx,
    const struct entry_list *list, const struct display_info *info,
    bool across);
static void print_stream(const struct context *ctx,
    const struct entry_list *list, const struct display_info *info);
static void print_long(const struct context *ctx,
    const struct entry_list *list, const struct display_info *info);
static size_t print_entry_short(const struct context *ctx,
    const struct entry *entry, const struct display_info *info);
static bool output_is_single_column(const struct context *ctx,
    const struct entry_list *list, const struct display_info *info);

static void
compute_display_info(const struct context *ctx, const struct entry_list *list,
    struct display_info *info)
{
	size_t i;
	char *tmp;

	memset(info, 0, sizeof(*info));
	for (i = 0; i < list->len; i++) {
		const struct entry *entry;
		size_t width;

		entry = &list->items[i];
		width = measure_name(&ctx->opt, entry->name);
		if (ctx->opt.show_type && file_type_char(entry, ctx->opt.slash_only) != '\0')
			width++;
		if (width > info->max_name_width)
			info->max_name_width = width;
		if (ctx->opt.show_inode && numeric_width(entry->st.st_ino) > info->inode_width)
			info->inode_width = numeric_width(entry->st.st_ino);
		if (ctx->opt.show_blocks || ctx->opt.layout == LAYOUT_LONG)
			info->total_blocks += (uintmax_t)entry->st.st_blocks;
		if (ctx->opt.show_blocks) {
			tmp = format_block_count(entry->st.st_blocks, ctx->opt.block_units,
			    ctx->opt.thousands);
			width = strlen(tmp);
			if (width > info->block_width)
				info->block_width = width;
			free(tmp);
		}
		if (ctx->opt.layout == LAYOUT_LONG) {
			struct entry *mutable_entry;

			mutable_entry = (struct entry *)entry;
			ensure_owner_group(mutable_entry, &ctx->opt);
			if (numeric_width(entry->st.st_nlink) > info->links_width)
				info->links_width = numeric_width(entry->st.st_nlink);
			width = strlen(entry->user);
			if (width > info->user_width)
				info->user_width = width;
			width = strlen(entry->group);
			if (width > info->group_width)
				info->group_width = width;
			tmp = format_entry_size(entry, ctx->opt.human_readable,
			    ctx->opt.thousands);
			width = strlen(tmp);
			if (width > info->size_width)
				info->size_width = width;
			free(tmp);
		}
	}
}

static size_t
print_entry_short(const struct context *ctx, const struct entry *entry,
    const struct display_info *info)
{
	size_t width;
	char *tmp;
	char indicator;
	const char *start;
	const char *end;

	width = 0;
	if (ctx->opt.show_inode) {
		printf("%*ju ", (int)info->inode_width, (uintmax_t)entry->st.st_ino);
		width += info->inode_width + 1;
	}
	if (ctx->opt.show_blocks) {
		tmp = format_block_count(entry->st.st_blocks, ctx->opt.block_units,
		    ctx->opt.thousands);
		printf("%*s ", (int)info->block_width, tmp);
		width += info->block_width + 1;
		free(tmp);
	}
	start = color_start(ctx, entry);
	end = color_end(ctx);
	if (*start != '\0')
		fputs(start, stdout);
	width += print_name(&ctx->opt, entry->name);
	if (*start != '\0')
		fputs(end, stdout);
	indicator = ctx->opt.show_type ? file_type_char(entry, ctx->opt.slash_only) : '\0';
	if (indicator != '\0') {
		putchar(indicator);
		width++;
	}
	return (width);
}

static void
print_single(const struct context *ctx, const struct entry_list *list,
    const struct display_info *info)
{
	size_t i;

	for (i = 0; i < list->len; i++) {
		print_entry_short(ctx, &list->items[i], info);
		putchar('\n');
	}
}

static void
print_stream(const struct context *ctx, const struct entry_list *list,
    const struct display_info *info)
{
	size_t i;
	size_t used;
	size_t width;

	used = 0;
	for (i = 0; i < list->len; i++) {
		width = info->max_name_width + (ctx->opt.show_inode ? info->inode_width + 1 : 0) +
		    (ctx->opt.show_blocks ? info->block_width + 1 : 0);
		if (used != 0 && used + width + 2 >= ctx->opt.terminal_width) {
			putchar('\n');
			used = 0;
		}
		used += print_entry_short(ctx, &list->items[i], info);
		if (i + 1 != list->len) {
			fputs(", ", stdout);
			used += 2;
		}
	}
	if (used != 0)
		putchar('\n');
}

static bool
output_is_single_column(const struct context *ctx, const struct entry_list *list,
    const struct display_info *info)
{
	size_t col_width;
	size_t cols;

	if (ctx->opt.layout == LAYOUT_SINGLE || list->len <= 1)
		return (true);
	if (ctx->opt.layout != LAYOUT_COLUMNS)
		return (false);
	col_width = info->max_name_width;
	if (ctx->opt.show_inode)
		col_width += info->inode_width + 1;
	if (ctx->opt.show_blocks)
		col_width += info->block_width + 1;
	col_width += 2;
	if (ctx->opt.terminal_width < col_width * 2)
		return (true);
	cols = ctx->opt.terminal_width / col_width;
	return (cols <= 1);
}

static void
print_columns(const struct context *ctx, const struct entry_list *list,
    const struct display_info *info, bool across)
{
	const struct entry **table;
	size_t col_width;
	size_t count;
	size_t cols;
	size_t rows;
	size_t row;
	size_t col;
	size_t idx;
	size_t target;
	size_t printed;
	size_t spaces;

	count = list->len;
	if (count == 0)
		return;
	col_width = info->max_name_width;
	if (ctx->opt.show_inode)
		col_width += info->inode_width + 1;
	if (ctx->opt.show_blocks)
		col_width += info->block_width + 1;
	col_width += 2;
	if (ctx->opt.terminal_width < col_width * 2) {
		print_single(ctx, list, info);
		return;
	}
	cols = ctx->opt.terminal_width / col_width;
	if (cols == 0)
		cols = 1;
	rows = (count + cols - 1) / cols;
	table = xmalloc(count * sizeof(*table));
	for (idx = 0; idx < count; idx++)
		table[idx] = &list->items[idx];
	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			idx = across ? row * cols + col : col * rows + row;
			if (idx >= count)
				continue;
			printed = print_entry_short(ctx, table[idx], info);
			target = (col + 1) * col_width;
			if (col + 1 == cols || (across ? row * cols + col + 1 : (col + 1) * rows + row) >= count)
				continue;
			if (printed < target - col * col_width)
				spaces = target - col * col_width - printed;
			else
				spaces = 1;
			while (spaces-- > 0)
				putchar(' ');
		}
		putchar('\n');
	}
	free(table);
}

static void
print_long(const struct context *ctx, const struct entry_list *list,
    const struct display_info *info)
{
	size_t i;
	char mode[12];
	char *size;
	char *time_str;
	const char *start;
	const char *end;

	for (i = 0; i < list->len; i++) {
		struct entry *entry;

		entry = &list->items[i];
		mode_string(entry, mode);
		size = format_entry_size(entry, ctx->opt.human_readable,
		    ctx->opt.thousands);
		time_str = format_entry_time(ctx, entry);
		printf("%s %*ju ", mode, (int)info->links_width,
		    (uintmax_t)entry->st.st_nlink);
		if (!ctx->opt.suppress_owner)
			printf("%-*s ", (int)info->user_width, entry->user);
		printf("%-*s %*s %s ", (int)info->group_width, entry->group,
		    (int)info->size_width, size, time_str);
		start = color_start(ctx, entry);
		end = color_end(ctx);
		if (*start != '\0')
			fputs(start, stdout);
		print_name(&ctx->opt, entry->name);
		if (*start != '\0')
			fputs(end, stdout);
		if (ctx->opt.show_type) {
			char indicator;

			indicator = file_type_char(entry, ctx->opt.slash_only);
			if (indicator != '\0')
				putchar(indicator);
		}
		if (entry->is_symlink && !entry->followed) {
			if (ensure_link_target(entry) == 0) {
				fputs(" -> ", stdout);
				print_name(&ctx->opt, entry->link_target);
			}
		}
		putchar('\n');
		free(size);
		free(time_str);
	}
}

void
print_entries(struct context *ctx, const struct entry_list *list,
    bool directory_listing)
{
	struct display_info info;
	char *total;

	compute_display_info(ctx, list, &info);
	if (directory_listing &&
	    (ctx->opt.layout == LAYOUT_LONG ||
	    (ctx->opt.show_blocks && !output_is_single_column(ctx, list, &info)))) {
		total = format_block_count((blkcnt_t)info.total_blocks,
		    ctx->opt.block_units, false);
		printf("total %s\n", total);
		free(total);
	}
	switch (ctx->opt.layout) {
	case LAYOUT_LONG:
		print_long(ctx, list, &info);
		break;
	case LAYOUT_STREAM:
		print_stream(ctx, list, &info);
		break;
	case LAYOUT_COLUMNS:
		print_columns(ctx, list, &info, ctx->opt.sort_across);
		break;
	case LAYOUT_SINGLE:
	default:
		print_single(ctx, list, &info);
		break;
	}
	ctx->wrote_output = true;
}
