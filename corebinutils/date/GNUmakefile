.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/date
OBJS := $(OBJDIR)/date.o $(OBJDIR)/vary.o

.PHONY: all clean dirs test status

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/date.o: $(CURDIR)/date.c $(CURDIR)/vary.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/date.c" -o "$@"

$(OBJDIR)/vary.o: $(CURDIR)/vary.c $(CURDIR)/vary.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/vary.c" -o "$@"

test: $(TARGET)
	DATE_BIN="$(TARGET)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(CURDIR)/build" "$(CURDIR)/out"
