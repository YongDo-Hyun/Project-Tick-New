.DEFAULT_GOAL := all

CC ?= cc
CPPFLAGS += -D_GNU_SOURCE -I$(CURDIR)
CFLAGS ?= -O2
CFLAGS += -std=c17 -g -Wall -Wextra -Werror -Wno-unused-parameter
LDFLAGS ?=
LDLIBS ?=

OBJDIR := $(CURDIR)/build
OUTDIR := $(CURDIR)/out
TARGET := $(OUTDIR)/ed
SOURCES := \
	$(CURDIR)/buf.c \
	$(CURDIR)/compat.c \
	$(CURDIR)/glbl.c \
	$(CURDIR)/io.c \
	$(CURDIR)/main.c \
	$(CURDIR)/re.c \
	$(CURDIR)/sub.c \
	$(CURDIR)/undo.c
OBJS := \
	$(OBJDIR)/buf.o \
	$(OBJDIR)/compat.o \
	$(OBJDIR)/glbl.o \
	$(OBJDIR)/io.o \
	$(OBJDIR)/main.o \
	$(OBJDIR)/re.o \
	$(OBJDIR)/sub.o \
	$(OBJDIR)/undo.o

.PHONY: all clean dirs status test

all: $(TARGET)

dirs:
	@mkdir -p "$(OBJDIR)" "$(OUTDIR)"

$(TARGET): $(OBJS) | dirs
	$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LDLIBS)

$(OBJDIR)/buf.o: $(CURDIR)/buf.c $(CURDIR)/ed.h $(CURDIR)/compat.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/buf.c" -o "$@"

$(OBJDIR)/compat.o: $(CURDIR)/compat.c $(CURDIR)/compat.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/compat.c" -o "$@"

$(OBJDIR)/glbl.o: $(CURDIR)/glbl.c $(CURDIR)/ed.h $(CURDIR)/compat.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/glbl.c" -o "$@"

$(OBJDIR)/io.o: $(CURDIR)/io.c $(CURDIR)/ed.h $(CURDIR)/compat.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/io.c" -o "$@"

$(OBJDIR)/main.o: $(CURDIR)/main.c $(CURDIR)/ed.h $(CURDIR)/compat.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/main.c" -o "$@"

$(OBJDIR)/re.o: $(CURDIR)/re.c $(CURDIR)/ed.h $(CURDIR)/compat.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/re.c" -o "$@"

$(OBJDIR)/sub.o: $(CURDIR)/sub.c $(CURDIR)/ed.h $(CURDIR)/compat.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/sub.c" -o "$@"

$(OBJDIR)/undo.o: $(CURDIR)/undo.c $(CURDIR)/ed.h $(CURDIR)/compat.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$(CURDIR)/undo.c" -o "$@"

test: $(TARGET)
	ED_BIN="$(TARGET)" CC="$(CC)" sh "$(CURDIR)/tests/test.sh"

status:
	@printf '%s\n' "$(TARGET)"

clean:
	@rm -rf "$(OBJDIR)" "$(OUTDIR)"
