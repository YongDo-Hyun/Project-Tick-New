/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
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

#include <stdlib.h>
#include <string.h>

#include "ls.h"
#include "extern.h"

static struct context *sort_ctx;
static bool sort_root_operands;

static int compare_names(const struct context *ctx, const struct entry *left,
    const struct entry *right)
{
	int cmp;

	(void)ctx;
	cmp = strcoll(left->name, right->name);
	if (cmp == 0)
		cmp = strcmp(left->name, right->name);
	return (cmp);
}

static int compare_version(const struct context *ctx, const struct entry *left,
    const struct entry *right)
{
	int cmp;

	(void)ctx;
	cmp = natural_version_compare(left->name, right->name);
	if (cmp == 0)
		cmp = compare_names(ctx, left, right);
	return (cmp);
}

static int compare_dir_groups(const struct context *ctx,
    const struct entry *left, const struct entry *right)
{
	if (ctx->opt.group_dirs == GROUP_NONE || left->is_dir == right->is_dir)
		return (0);
	if (ctx->opt.group_dirs == GROUP_DIRS_FIRST)
		return (left->is_dir ? -1 : 1);
	return (left->is_dir ? 1 : -1);
}

static int compare_times(const struct context *ctx, const struct entry *left,
    const struct entry *right)
{
	struct timespec lhs;
	struct timespec rhs;
	bool tie_reverse;
	int cmp;

	cmp = compare_names(ctx, left, right);
	if (!selected_time(ctx, left, &lhs) || !selected_time(ctx, right, &rhs))
		return (cmp);
	if (lhs.tv_sec < rhs.tv_sec)
		cmp = 1;
	else if (lhs.tv_sec > rhs.tv_sec)
		cmp = -1;
	else if (lhs.tv_nsec < rhs.tv_nsec)
		cmp = 1;
	else if (lhs.tv_nsec > rhs.tv_nsec)
		cmp = -1;
	else {
		tie_reverse = ctx->opt.reverse_sort ? !ctx->opt.same_sort_direction :
		    ctx->opt.same_sort_direction;
		cmp = compare_names(ctx, left, right);
		if (tie_reverse)
			cmp = -cmp;
		return (cmp);
	}
	if (ctx->opt.reverse_sort)
		cmp = -cmp;
	return (cmp);
}

static int compare_sizes(const struct context *ctx, const struct entry *left,
    const struct entry *right)
{
	if (left->st.st_size < right->st.st_size)
		return (1);
	if (left->st.st_size > right->st.st_size)
		return (-1);
	return (compare_names(ctx, left, right));
}

static int compare_entries(const struct context *ctx, const struct entry *left,
    const struct entry *right, bool root_operands)
{
	int cmp;

	if (root_operands && !ctx->opt.list_directory_itself &&
	    left->is_dir != right->is_dir)
		return (left->is_dir ? 1 : -1);
	cmp = compare_dir_groups(ctx, left, right);
	if (cmp != 0)
		return (cmp);
	switch (ctx->opt.sort) {
	case SORT_TIME:
		cmp = compare_times(ctx, left, right);
		break;
	case SORT_SIZE:
		cmp = compare_sizes(ctx, left, right);
		break;
	case SORT_VERSION:
		cmp = compare_version(ctx, left, right);
		break;
	case SORT_NAME:
	default:
		cmp = compare_names(ctx, left, right);
		break;
	}
	if (ctx->opt.reverse_sort && ctx->opt.sort != SORT_TIME)
		cmp = -cmp;
	return (cmp);
}

static int
qsort_compare(const void *lhs, const void *rhs)
{
	const struct entry *left;
	const struct entry *right;

	left = lhs;
	right = rhs;
	return (compare_entries(sort_ctx, left, right, sort_root_operands));
}

void
sort_entries(struct context *ctx, struct entry_list *list, bool root_operands)
{
	if (ctx->opt.no_sort || list->len < 2)
		return;
	sort_ctx = ctx;
	sort_root_operands = root_operands;
	qsort(list->items, list->len, sizeof(list->items[0]), qsort_compare);
}
