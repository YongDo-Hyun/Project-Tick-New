/* SPDX-FileCopyrightText: 2006-2014 cgit Development Team
 * SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#ifndef CMD_H
#define CMD_H

typedef void (*cgit_cmd_fn)(void);

struct cgit_cmd {
	const char *name;
	cgit_cmd_fn fn;
	unsigned int want_repo:1,
		want_vpath:1,
		is_clone:1;
};

extern struct cgit_cmd *cgit_get_cmd(void);

#endif /* CMD_H */
