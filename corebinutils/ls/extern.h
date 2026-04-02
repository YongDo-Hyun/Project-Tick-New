/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 2026
 *  Project Tick. All rights reserved.
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

#ifndef LS_EXTERN_H
#define LS_EXTERN_H

#include "ls.h"

void *xmalloc(size_t size);
void *xreallocarray(void *ptr, size_t nmemb, size_t size);
char *xstrdup(const char *src);
char *xasprintf(const char *fmt, ...);
char *join_path(const char *parent, const char *name);
void free_entry(struct entry *entry);
void free_entry_list(struct entry_list *list);
void append_entry(struct entry_list *list, struct entry *entry);
size_t numeric_width(uintmax_t value);
int print_name(const struct options *opt, const char *name);
size_t measure_name(const struct options *opt, const char *name);
char file_type_char(const struct entry *entry, bool slash_only);
void mode_string(const struct entry *entry, char buf[12]);
char *format_uintmax(uintmax_t value, bool thousands);
char *format_block_count(blkcnt_t blocks, long units, bool thousands);
char *format_entry_size(const struct entry *entry, bool human, bool thousands);
int ensure_owner_group(struct entry *entry, const struct options *opt);
int ensure_link_target(struct entry *entry);
bool selected_time(const struct context *ctx, const struct entry *entry,
    struct timespec *ts);
char *format_entry_time(const struct context *ctx, const struct entry *entry);
const char *color_start(const struct context *ctx, const struct entry *entry);
const char *color_end(const struct context *ctx);
int natural_version_compare(const char *left, const char *right);
void sort_entries(struct context *ctx, struct entry_list *list,
    bool root_operands);
void print_entries(struct context *ctx, const struct entry_list *list,
    bool directory_listing);
void usage(void);

#endif
