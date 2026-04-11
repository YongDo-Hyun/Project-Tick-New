# SPDX-FileCopyrightText: 2006-2014 cgit Development Team
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: GPL-2.0-only
include ../git/config.mak.uname
-include ../cgit.conf

SHELL_PATH ?= $(SHELL)
SHELL_PATH_SQ = $(subst ','\'',$(SHELL_PATH))

T = $(wildcard t[0-9][0-9][0-9][0-9]-*.sh)

all: $(T)

$(T):
	@'$(SHELL_PATH_SQ)' $@ $(CGIT_TEST_OPTS)

clean:
	$(RM) -rf trash

.PHONY: $(T) clean
