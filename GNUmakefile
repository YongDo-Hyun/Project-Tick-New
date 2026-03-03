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
TARGET := $(OUTDIR)/rm
UNLINK_TARGET := $(OUTDIR)/unlink
OBJS := $(OBJDIR)/rm.o

.PHONY: all clean dirs status test

all: $(TARGET) $(UNLINK_TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(UNLINK_TARGET): $(TARGET) | dirs
	ln -sf "rm" "$@"

$(OBJDIR)/rm.o: $(CURDIR)/rm.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/rm.c" -o "$@"

test: $(TARGET) $(UNLINK_TARGET)
	RM_BIN="$(TARGET)" UNLINK_BIN="$(UNLINK_TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
