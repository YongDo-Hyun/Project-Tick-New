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

#ifndef LS_H
#define LS_H

#include <sys/stat.h>
#include <sys/types.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

enum layout_mode {
	LAYOUT_SINGLE = 0,
	LAYOUT_COLUMNS,
	LAYOUT_LONG,
	LAYOUT_STREAM,
};

enum sort_mode {
	SORT_NAME = 0,
	SORT_TIME,
	SORT_SIZE,
	SORT_VERSION,
};

enum time_field {
	TIME_MTIME = 0,
	TIME_ATIME,
	TIME_BTIME,
	TIME_CTIME,
};

enum follow_mode {
	FOLLOW_DEFAULT = 0,
	FOLLOW_COMMAND_LINE,
	FOLLOW_ALL,
	FOLLOW_NEVER,
};

enum group_mode {
	GROUP_NONE = 0,
	GROUP_DIRS_FIRST,
	GROUP_DIRS_LAST,
};

enum name_mode {
	NAME_LITERAL = 0,
	NAME_PRINTABLE,
	NAME_OCTAL,
	NAME_ESCAPE,
};

enum color_mode {
	COLOR_DEFAULT = 0,
	COLOR_NEVER,
	COLOR_AUTO,
	COLOR_ALWAYS,
};

struct file_time {
	struct timespec ts;
	bool available;
};

struct options {
	enum layout_mode layout;
	enum sort_mode sort;
	enum time_field time_field;
	enum follow_mode follow;
	enum group_mode group_dirs;
	enum name_mode name_mode;
	enum color_mode color_mode;
	long block_units;
	size_t terminal_width;
	const char *time_format;
	bool stdout_is_tty;
	bool show_all;
	bool show_almost_all;
	bool disable_root_almost_all;
	bool list_directory_itself;
	bool recursive;
	bool show_full_time;
	bool human_readable;
	bool show_inode;
	bool show_blocks;
	bool show_type;
	bool slash_only;
	bool numeric_ids;
	bool suppress_owner;
	bool reverse_sort;
	bool same_sort_direction;
	bool no_sort;
	bool sort_across;
	bool thousands;
	bool colorize;
};

struct entry {
	char *name;
	char *path;
	struct stat st;
	struct file_time btime;
	char *user;
	char *group;
	char *link_target;
	int stat_errno;
	bool stat_ok;
	bool is_dir;
	bool is_symlink;
	bool followed;
};

struct entry_list {
	struct entry *items;
	size_t len;
	size_t cap;
};

struct display_info {
	uintmax_t total_blocks;
	size_t max_name_width;
	size_t inode_width;
	size_t block_width;
	size_t links_width;
	size_t user_width;
	size_t group_width;
	size_t size_width;
};

struct context {
	struct options opt;
	time_t now;
	int exit_status;
	bool wrote_output;
};

struct visit_stack {
	dev_t dev;
	ino_t ino;
	const struct visit_stack *parent;
};

#endif
