.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS ?=
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Werror
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/mkdir
OBJS := $(OBJDIR)/mkdir.o $(OBJDIR)/mode.o

.PHONY: all clean dirs status test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/mkdir.o: $(CURDIR)/mkdir.c $(CURDIR)/mode.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/mkdir.c" -o "$@"

$(OBJDIR)/mode.o: $(CURDIR)/mode.c $(CURDIR)/mode.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/mode.c" -o "$@"

test: $(TARGET)
	MKDIR_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
