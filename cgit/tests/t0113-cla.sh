#!/bin/sh
# SPDX-FileCopyrightText: 2006-2014 cgit Development Team
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: GPL-2.0-only

test_description='Check top-level CLA page'
. ./setup.sh

test_expect_success 'index has cla tab' '
	cgit_url "" >tmp &&
	grep "p=cla" tmp &&
	grep "Contributor License Agreement" tmp
'

test_expect_success 'generate top-level cla page' '
	cgit_query "p=cla" >tmp
'

test_expect_success 'find cla content' '
	grep "site contributor license agreement" tmp
'

test_expect_success 'repo cla renders top-level cla page' '
	cgit_url "foo/cla" >tmp &&
	grep "site contributor license agreement" tmp
'

test_done
