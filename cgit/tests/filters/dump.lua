-- SPDX-FileCopyrightText: 2006-2014 cgit Development Team
-- SPDX-FileCopyrightText: 2026 Project Tick
-- SPDX-FileContributor: Project Tick
-- SPDX-License-Identifier: GPL-2.0-only

function filter_open(...)
	buffer = ""
	for i = 1, select("#", ...) do
		buffer = buffer .. select(i, ...) .. " "
	end
end

function filter_close()
	html(buffer)
	return 0
end

function filter_write(str)
	buffer = buffer .. string.upper(str)
end


