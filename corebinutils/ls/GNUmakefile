.DEFAULT_GOAL := all

CC ?= cc
CCACHE_DISABLE ?= 1
CPPFLAGS += -D_GNU_SOURCE -I$(CURDIR)
CFLAGS ?= -O2
CFLAGS += -std=c11 -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/ls
OBJS := $(OBJDIR)/ls.o $(OBJDIR)/cmp.o $(OBJDIR)/print.o $(OBJDIR)/util.o

.PHONY: all clean dirs test status

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	env CCACHE_DISABLE=$(CCACHE_DISABLE) $(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/ls.o: $(CURDIR)/ls.c $(CURDIR)/ls.h $(CURDIR)/extern.h | dirs
	env CCACHE_DISABLE=$(CCACHE_DISABLE) $(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/ls.c" -o "$@"

$(OBJDIR)/cmp.o: $(CURDIR)/cmp.c $(CURDIR)/ls.h $(CURDIR)/extern.h | dirs
	env CCACHE_DISABLE=$(CCACHE_DISABLE) $(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/cmp.c" -o "$@"

$(OBJDIR)/print.o: $(CURDIR)/print.c $(CURDIR)/ls.h $(CURDIR)/extern.h | dirs
	env CCACHE_DISABLE=$(CCACHE_DISABLE) $(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/print.c" -o "$@"

$(OBJDIR)/util.o: $(CURDIR)/util.c $(CURDIR)/ls.h $(CURDIR)/extern.h | dirs
	env CCACHE_DISABLE=$(CCACHE_DISABLE) $(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/util.c" -o "$@"

test: $(TARGET)
	LS_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(CURDIR)/build" "$(CURDIR)/out"
