-- This script is an example of an owner-filter.  It replaces the
-- usual query link with one to a fictional homepage.  This script may
-- be used with the owner-filter or repo.owner-filter settings in
-- cgitrc with the `lua:` prefix.
-- SPDX-FileCopyrightText: 2006-2014 cgit Development Team
-- SPDX-FileCopyrightText: 2026 Project Tick
-- SPDX-FileContributor: Project Tick
-- SPDX-License-Identifier: GPL-2.0-only

function filter_open()
	buffer = ""
end

function filter_close()
	html(string.format("<a href=\"%s\">%s</a>", "http://wiki.example.com/about/" .. buffer, buffer))
	return 0
end

function filter_write(str)
	buffer = buffer .. str
end
