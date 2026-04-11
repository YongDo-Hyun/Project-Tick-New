#!/bin/sh
# SPDX-FileCopyrightText: 2006-2014 cgit Development Team
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: GPL-2.0-only
[ "$#" -gt 0 ] && printf "%s " "$*"
tr '[:lower:]' '[:upper:]'
