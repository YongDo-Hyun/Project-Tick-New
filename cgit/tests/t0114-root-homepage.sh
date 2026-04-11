#!/bin/sh
# SPDX-FileCopyrightText: 2006-2014 cgit Development Team
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: GPL-2.0-only

test_description='Check root external links'
. ./setup.sh

test_expect_success 'index has configured root links' '
	cgit_url "" >tmp &&
	grep "https://projecttick.org" tmp &&
	grep ">Project Tick<" tmp &&
	grep "https://github.com/example" tmp &&
	grep ">GitHub<" tmp &&
	grep "https://gitlab.com/example" tmp &&
	grep ">GitLab<" tmp &&
	grep "https://codeberg.org/example" tmp &&
	grep ">Codeberg<" tmp
'

test_expect_success 'root pages keep root links' '
	cgit_query "p=coc" >tmp &&
	grep "https://projecttick.org" tmp &&
	grep ">Project Tick<" tmp &&
	grep "https://github.com/example" tmp &&
	grep ">GitHub<" tmp &&
	grep "https://gitlab.com/example" tmp &&
	grep ">GitLab<" tmp &&
	grep "https://codeberg.org/example" tmp &&
	grep ">Codeberg<" tmp
'

test_done
