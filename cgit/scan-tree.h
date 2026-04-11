/* SPDX-FileCopyrightText: 2006-2014 cgit Development Team
 * SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */
extern void scan_projects(const char *path, const char *projectsfile, repo_config_fn fn);
extern void scan_tree(const char *path, repo_config_fn fn);
