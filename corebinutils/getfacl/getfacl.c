/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1999, 2001, 2002 Robert N M Watson
 * All rights reserved.
 *
 * Copyright (c) 2026
 *  Project Tick. All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Linux-native getfacl implementation.
 *
 * FreeBSD's original utility depends on sys/acl.h and acl_*_np interfaces.
 * This port reads Linux POSIX ACLs directly from xattrs instead:
 *   - system.posix_acl_access
 *   - system.posix_acl_default
 *
 * The goal is a native Linux implementation that builds cleanly with musl
 * without a FreeBSD ACL compatibility layer.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <endian.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef le16toh
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le16toh(x) (x)
#define le32toh(x) (x)
#else
#define le16toh(x) __builtin_bswap16(x)
#define le32toh(x) __builtin_bswap32(x)
#endif
#endif

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

#define ACL_XATTR_ACCESS "system.posix_acl_access"
#define ACL_XATTR_DEFAULT "system.posix_acl_default"

#define POSIX_ACL_XATTR_VERSION 0x0002U
#define ACL_UNDEFINED_ID ((uint32_t)-1)

#define ACL_READ 0x04U
#define ACL_WRITE 0x02U
#define ACL_EXECUTE 0x01U

#define ACL_USER_OBJ 0x01U
#define ACL_USER 0x02U
#define ACL_GROUP_OBJ 0x04U
#define ACL_GROUP 0x08U
#define ACL_MASK 0x10U
#define ACL_OTHER 0x20U

struct posix_acl_xattr_entry_linux {
	uint16_t e_tag;
	uint16_t e_perm;
	uint32_t e_id;
};

struct posix_acl_xattr_header_linux {
	uint32_t a_version;
};

enum acl_kind {
	ACL_KIND_ACCESS,
	ACL_KIND_DEFAULT,
};

enum xattr_result {
	XATTR_RESULT_ERROR = -1,
	XATTR_RESULT_ABSENT = 0,
	XATTR_RESULT_PRESENT = 1,
};

struct options {
	bool default_acl;
	bool no_follow;
	bool numeric_ids;
	bool omit_header;
	bool skip_base;
};

struct acl_entry_linux {
	uint16_t tag;
	uint16_t perm;
	uint32_t id;
};

struct acl_blob {
	struct acl_entry_linux *entries;
	size_t count;
};

struct acl_shape {
	bool has_mask;
	bool has_named_user;
	bool has_named_group;
	uint16_t user_obj_perm;
	uint16_t group_obj_perm;
	uint16_t other_perm;
	uint16_t mask_perm;
};

struct output_state {
	size_t sections_emitted;
};

static const struct option long_options[] = {
	{ "default", no_argument, NULL, 'd' },
	{ "numeric", no_argument, NULL, 'n' },
	{ "omit-header", no_argument, NULL, 'q' },
	{ "skip-base", no_argument, NULL, 's' },
	{ NULL, 0, NULL, 0 },
};

static void usage(void) __attribute__((noreturn));
static int process_path(const char *path, const struct options *opts,
    struct output_state *state);
static int process_stdin(const struct options *opts, struct output_state *state);
static int stat_path(const char *path, bool no_follow, struct stat *st);
static int load_acl_xattr(const char *path, enum acl_kind kind, bool no_follow,
    void **buf_out, size_t *size_out);
static int parse_acl_blob(const void *buf, size_t size, struct acl_blob *acl,
    const char **error_out);
static void free_acl_blob(struct acl_blob *acl);
static int compare_acl_entries(const void *lhs, const void *rhs);
static int validate_acl_blob(const struct acl_blob *acl, struct acl_shape *shape,
    const char **error_out);
static bool acl_is_trivial(const struct acl_shape *shape, mode_t mode);
static int emit_access_acl(const char *path, const struct stat *st,
    const struct options *opts, const struct acl_blob *acl,
    const struct acl_shape *shape, bool have_acl, struct output_state *state);
static int emit_default_acl(const char *path, const struct stat *st,
    const struct options *opts, const struct acl_blob *acl, bool have_acl,
    struct output_state *state);
static void begin_section_if_needed(struct output_state *state);
static void print_header(const char *path, const struct stat *st,
    bool numeric_ids);
static void print_acl_entries(const struct acl_blob *acl, enum acl_kind kind,
    bool numeric_ids);
static void print_synthesized_access_acl(mode_t mode);
static const char *tag_name(uint16_t tag);
static void perm_string(uint16_t perm, char out[4]);
static const char *format_uid(uid_t uid, bool numeric, char *buf, size_t buflen);
static const char *format_gid(gid_t gid, bool numeric, char *buf, size_t buflen);

static void
usage(void)
{

	fprintf(stderr, "usage: getfacl [-dhinqsv] [file ...]\n");
	exit(1);
}

static int
stat_path(const char *path, bool no_follow, struct stat *st)
{

	if (no_follow)
		return (lstat(path, st));
	return (stat(path, st));
}

static int
load_acl_xattr(const char *path, enum acl_kind kind, bool no_follow, void **buf_out,
    size_t *size_out)
{
	const char *name;
	ssize_t size;
	void *buf;

	*buf_out = NULL;
	*size_out = 0;
	name = (kind == ACL_KIND_DEFAULT) ? ACL_XATTR_DEFAULT : ACL_XATTR_ACCESS;

	for (;;) {
		if (no_follow)
			size = lgetxattr(path, name, NULL, 0);
		else
			size = getxattr(path, name, NULL, 0);
		if (size >= 0)
			break;
		if (errno == ENODATA || errno == ENOATTR || errno == ENOTSUP ||
		    errno == EOPNOTSUPP)
			return (XATTR_RESULT_ABSENT);
		return (XATTR_RESULT_ERROR);
	}

	if (size == 0)
		return (XATTR_RESULT_ABSENT);

	buf = malloc((size_t)size);
	if (buf == NULL)
		err(1, "malloc");

	for (;;) {
		ssize_t nread;

		if (no_follow)
			nread = lgetxattr(path, name, buf, (size_t)size);
		else
			nread = getxattr(path, name, buf, (size_t)size);
		if (nread >= 0) {
			*buf_out = buf;
			*size_out = (size_t)nread;
			return (XATTR_RESULT_PRESENT);
		}
		if (errno != ERANGE) {
			free(buf);
			if (errno == ENODATA || errno == ENOATTR || errno == ENOTSUP ||
			    errno == EOPNOTSUPP)
				return (XATTR_RESULT_ABSENT);
			return (XATTR_RESULT_ERROR);
		}
		free(buf);
		if (no_follow)
			size = lgetxattr(path, name, NULL, 0);
		else
			size = getxattr(path, name, NULL, 0);
		if (size < 0) {
			if (errno == ENODATA || errno == ENOATTR || errno == ENOTSUP ||
			    errno == EOPNOTSUPP)
				return (XATTR_RESULT_ABSENT);
			return (XATTR_RESULT_ERROR);
		}
		buf = malloc((size_t)size);
		if (buf == NULL)
			err(1, "malloc");
	}
}

static int
compare_acl_entries(const void *lhs, const void *rhs)
{
	const struct acl_entry_linux *a, *b;
	int order_a, order_b;

	a = lhs;
	b = rhs;

	switch (a->tag) {
	case ACL_USER_OBJ:
		order_a = 0;
		break;
	case ACL_USER:
		order_a = 1;
		break;
	case ACL_GROUP_OBJ:
		order_a = 2;
		break;
	case ACL_GROUP:
		order_a = 3;
		break;
	case ACL_MASK:
		order_a = 4;
		break;
	case ACL_OTHER:
		order_a = 5;
		break;
	default:
		order_a = 6;
		break;
	}

	switch (b->tag) {
	case ACL_USER_OBJ:
		order_b = 0;
		break;
	case ACL_USER:
		order_b = 1;
		break;
	case ACL_GROUP_OBJ:
		order_b = 2;
		break;
	case ACL_GROUP:
		order_b = 3;
		break;
	case ACL_MASK:
		order_b = 4;
		break;
	case ACL_OTHER:
		order_b = 5;
		break;
	default:
		order_b = 6;
		break;
	}

	if (order_a != order_b)
		return (order_a - order_b);
	if (a->id < b->id)
		return (-1);
	if (a->id > b->id)
		return (1);
	return (0);
}

static int
parse_acl_blob(const void *buf, size_t size, struct acl_blob *acl,
    const char **error_out)
{
	const struct posix_acl_xattr_header_linux *header;
	const struct posix_acl_xattr_entry_linux *src;
	size_t payload_size, count, i;

	memset(acl, 0, sizeof(*acl));

	if (size < sizeof(*header)) {
		*error_out = "truncated header";
		return (-1);
	}

	header = buf;
	if (le32toh(header->a_version) != POSIX_ACL_XATTR_VERSION) {
		*error_out = "unsupported ACL xattr version";
		return (-1);
	}

	payload_size = size - sizeof(*header);
	if (payload_size % sizeof(*src) != 0) {
		*error_out = "truncated ACL entry";
		return (-1);
	}

	count = payload_size / sizeof(*src);
	if (count == 0) {
		*error_out = "empty ACL";
		return (-1);
	}

	acl->entries = calloc(count, sizeof(*acl->entries));
	if (acl->entries == NULL)
		err(1, "calloc");
	acl->count = count;

	src = (const struct posix_acl_xattr_entry_linux *)((const char *)buf +
	    sizeof(*header));
	for (i = 0; i < count; i++) {
		acl->entries[i].tag = le16toh(src[i].e_tag);
		acl->entries[i].perm = le16toh(src[i].e_perm);
		acl->entries[i].id = le32toh(src[i].e_id);
	}

	qsort(acl->entries, acl->count, sizeof(*acl->entries), compare_acl_entries);
	return (0);
}

static int
validate_acl_blob(const struct acl_blob *acl, struct acl_shape *shape,
    const char **error_out)
{
	size_t i;
	bool have_user_obj, have_group_obj, have_other;

	memset(shape, 0, sizeof(*shape));
	have_user_obj = false;
	have_group_obj = false;
	have_other = false;

	for (i = 0; i < acl->count; i++) {
		const struct acl_entry_linux *entry;

		entry = &acl->entries[i];
		if ((entry->perm & ~(ACL_READ | ACL_WRITE | ACL_EXECUTE)) != 0) {
			*error_out = "invalid permission bits";
			return (-1);
		}

		switch (entry->tag) {
		case ACL_USER_OBJ:
			if (have_user_obj) {
				*error_out = "duplicate user:: entry";
				return (-1);
			}
			have_user_obj = true;
			shape->user_obj_perm = entry->perm;
			break;
		case ACL_USER:
			if (entry->id == (uint32_t)ACL_UNDEFINED_ID) {
				*error_out = "named user entry without id";
				return (-1);
			}
			if (i > 0 && acl->entries[i - 1].tag == ACL_USER &&
			    acl->entries[i - 1].id == entry->id) {
				*error_out = "duplicate named user entry";
				return (-1);
			}
			shape->has_named_user = true;
			break;
		case ACL_GROUP_OBJ:
			if (have_group_obj) {
				*error_out = "duplicate group:: entry";
				return (-1);
			}
			have_group_obj = true;
			shape->group_obj_perm = entry->perm;
			break;
		case ACL_GROUP:
			if (entry->id == (uint32_t)ACL_UNDEFINED_ID) {
				*error_out = "named group entry without id";
				return (-1);
			}
			if (i > 0 && acl->entries[i - 1].tag == ACL_GROUP &&
			    acl->entries[i - 1].id == entry->id) {
				*error_out = "duplicate named group entry";
				return (-1);
			}
			shape->has_named_group = true;
			break;
		case ACL_MASK:
			if (shape->has_mask) {
				*error_out = "duplicate mask:: entry";
				return (-1);
			}
			shape->has_mask = true;
			shape->mask_perm = entry->perm;
			break;
		case ACL_OTHER:
			if (have_other) {
				*error_out = "duplicate other:: entry";
				return (-1);
			}
			have_other = true;
			shape->other_perm = entry->perm;
			break;
		default:
			*error_out = "unknown ACL tag";
			return (-1);
		}
	}

	if (!have_user_obj || !have_group_obj || !have_other) {
		*error_out = "missing required ACL entry";
		return (-1);
	}
	if ((shape->has_named_user || shape->has_named_group) && !shape->has_mask) {
		*error_out = "named ACL entries require mask::";
		return (-1);
	}

	return (0);
}

static bool
acl_is_trivial(const struct acl_shape *shape, mode_t mode)
{
	uint16_t user_perm, group_perm, other_perm;

	if (shape->has_named_user || shape->has_named_group)
		return (false);

	user_perm = (uint16_t)((mode >> 6) & 0x7);
	group_perm = (uint16_t)((mode >> 3) & 0x7);
	other_perm = (uint16_t)(mode & 0x7);

	if (shape->user_obj_perm != user_perm ||
	    shape->group_obj_perm != group_perm ||
	    shape->other_perm != other_perm)
		return (false);
	if (shape->has_mask && shape->mask_perm != group_perm)
		return (false);
	return (true);
}

static void
free_acl_blob(struct acl_blob *acl)
{

	free(acl->entries);
	acl->entries = NULL;
	acl->count = 0;
}

static void
begin_section_if_needed(struct output_state *state)
{

	if (state->sections_emitted > 0)
		printf("\n");
	state->sections_emitted++;
}

static const char *
format_uid(uid_t uid, bool numeric, char *buf, size_t buflen)
{
	struct passwd *pw;

	if (!numeric) {
		pw = getpwuid(uid);
		if (pw != NULL)
			return (pw->pw_name);
	}
	snprintf(buf, buflen, "%lu", (unsigned long)uid);
	return (buf);
}

static const char *
format_gid(gid_t gid, bool numeric, char *buf, size_t buflen)
{
	struct group *gr;

	if (!numeric) {
		gr = getgrgid(gid);
		if (gr != NULL)
			return (gr->gr_name);
	}
	snprintf(buf, buflen, "%lu", (unsigned long)gid);
	return (buf);
}

static void
print_header(const char *path, const struct stat *st, bool numeric_ids)
{
	char owner_buf[32], group_buf[32];

	printf("# file: %s\n", path);
	printf("# owner: %s\n", format_uid(st->st_uid, numeric_ids, owner_buf,
	    sizeof(owner_buf)));
	printf("# group: %s\n", format_gid(st->st_gid, numeric_ids, group_buf,
	    sizeof(group_buf)));
}

static void
perm_string(uint16_t perm, char out[4])
{

	out[0] = (perm & ACL_READ) ? 'r' : '-';
	out[1] = (perm & ACL_WRITE) ? 'w' : '-';
	out[2] = (perm & ACL_EXECUTE) ? 'x' : '-';
	out[3] = '\0';
}

static const char *
tag_name(uint16_t tag)
{

	switch (tag) {
	case ACL_USER_OBJ:
	case ACL_USER:
		return ("user");
	case ACL_GROUP_OBJ:
	case ACL_GROUP:
		return ("group");
	case ACL_MASK:
		return ("mask");
	case ACL_OTHER:
		return ("other");
	default:
		return ("unknown");
	}
}

static void
print_acl_entries(const struct acl_blob *acl, enum acl_kind kind, bool numeric_ids)
{
	size_t i;

	for (i = 0; i < acl->count; i++) {
		const struct acl_entry_linux *entry;
		char qualifier_buf[32], perms[4];
		const char *prefix, *qualifier;

		entry = &acl->entries[i];
		prefix = (kind == ACL_KIND_DEFAULT) ? "default:" : "";
		perm_string(entry->perm, perms);

		switch (entry->tag) {
		case ACL_USER_OBJ:
		case ACL_GROUP_OBJ:
		case ACL_MASK:
		case ACL_OTHER:
			printf("%s%s::%s\n", prefix, tag_name(entry->tag), perms);
			break;
		case ACL_USER:
			qualifier = format_uid((uid_t)entry->id, numeric_ids,
			    qualifier_buf, sizeof(qualifier_buf));
			printf("%suser:%s:%s\n", prefix, qualifier, perms);
			break;
		case ACL_GROUP:
			qualifier = format_gid((gid_t)entry->id, numeric_ids,
			    qualifier_buf, sizeof(qualifier_buf));
			printf("%sgroup:%s:%s\n", prefix, qualifier, perms);
			break;
		default:
			break;
		}
	}
}

static void
print_synthesized_access_acl(mode_t mode)
{
	char perms[4];

	perm_string((uint16_t)((mode >> 6) & 0x7), perms);
	printf("user::%s\n", perms);

	perm_string((uint16_t)((mode >> 3) & 0x7), perms);
	printf("group::%s\n", perms);

	perm_string((uint16_t)(mode & 0x7), perms);
	printf("other::%s\n", perms);
}

static int
emit_access_acl(const char *path, const struct stat *st, const struct options *opts,
    const struct acl_blob *acl, const struct acl_shape *shape, bool have_acl,
    struct output_state *state)
{
	bool trivial;

	trivial = !have_acl || acl_is_trivial(shape, st->st_mode);
	if (opts->skip_base && trivial)
		return (0);

	begin_section_if_needed(state);
	if (!opts->omit_header)
		print_header(path, st, opts->numeric_ids);
	if (have_acl)
		print_acl_entries(acl, ACL_KIND_ACCESS, opts->numeric_ids);
	else
		print_synthesized_access_acl(st->st_mode);
	return (0);
}

static int
emit_default_acl(const char *path, const struct stat *st, const struct options *opts,
    const struct acl_blob *acl, bool have_acl, struct output_state *state)
{
	bool have_output;

	if (!have_acl && opts->skip_base)
		return (0);

	have_output = !opts->omit_header || acl->count > 0;
	if (!have_output)
		return (0);

	begin_section_if_needed(state);
	if (!opts->omit_header)
		print_header(path, st, opts->numeric_ids);
	if (acl->count > 0)
		print_acl_entries(acl, ACL_KIND_DEFAULT, opts->numeric_ids);
	return (0);
}

static int
process_path(const char *path, const struct options *opts, struct output_state *state)
{
	struct stat st;
	struct acl_blob acl;
	struct acl_shape shape;
	const char *parse_error;
	void *raw_acl;
	size_t raw_size;
	int xattr_state;

	memset(&acl, 0, sizeof(acl));
	raw_acl = NULL;
	raw_size = 0;

	if (stat_path(path, opts->no_follow, &st) == -1) {
		warn("%s", path);
		return (1);
	}

	if (opts->no_follow && S_ISLNK(st.st_mode)) {
		warnx("%s: symbolic link ACLs are not supported on Linux", path);
		return (1);
	}

	xattr_state = load_acl_xattr(path,
	    opts->default_acl ? ACL_KIND_DEFAULT : ACL_KIND_ACCESS,
	    opts->no_follow, &raw_acl, &raw_size);
	if (xattr_state == XATTR_RESULT_ERROR) {
		warn("%s", path);
		return (1);
	}

	if (xattr_state == XATTR_RESULT_PRESENT) {
		if (parse_acl_blob(raw_acl, raw_size, &acl, &parse_error) != 0) {
			warnx("%s: invalid POSIX ACL xattr: %s", path, parse_error);
			free(raw_acl);
			return (1);
		}
		if (validate_acl_blob(&acl, &shape, &parse_error) != 0) {
			warnx("%s: invalid POSIX ACL xattr: %s", path, parse_error);
			free(raw_acl);
			free_acl_blob(&acl);
			return (1);
		}
	}
	free(raw_acl);

	if (opts->default_acl)
		xattr_state = emit_default_acl(path, &st, opts, &acl,
		    xattr_state == XATTR_RESULT_PRESENT, state);
	else
		xattr_state = emit_access_acl(path, &st, opts, &acl, &shape,
		    xattr_state == XATTR_RESULT_PRESENT, state);

	free_acl_blob(&acl);
	return (xattr_state);
}

static int
process_stdin(const struct options *opts, struct output_state *state)
{
	char *line;
	size_t cap;
	ssize_t len;
	int status;

	line = NULL;
	cap = 0;
	status = 0;

	while ((len = getline(&line, &cap, stdin)) != -1) {
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';
		if (line[0] == '\0') {
			warnx("stdin: empty pathname");
			status = 1;
			continue;
		}
		if (process_path(line, opts, state) != 0)
			status = 1;
	}

	if (ferror(stdin)) {
		warn("stdin");
		status = 1;
	}

	free(line);
	return (status);
}

int
main(int argc, char *argv[])
{
	struct options opts;
	struct output_state state;
	int ch, i, status;

	memset(&opts, 0, sizeof(opts));
	memset(&state, 0, sizeof(state));
	opterr = 0;

	while ((ch = getopt_long(argc, argv, "+dhinqsv", long_options, NULL)) != -1) {
		switch (ch) {
		case 'd':
			opts.default_acl = true;
			break;
		case 'h':
			opts.no_follow = true;
			break;
		case 'i':
			errx(1, "option -i is not supported on Linux");
		case 'n':
			opts.numeric_ids = true;
			break;
		case 'q':
			opts.omit_header = true;
			break;
		case 's':
			opts.skip_base = true;
			break;
		case 'v':
			errx(1, "option -v is not supported on Linux");
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		return (process_stdin(&opts, &state));

	status = 0;
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-") == 0) {
			if (process_stdin(&opts, &state) != 0)
				status = 1;
			continue;
		}
		if (process_path(argv[i], &opts, &state) != 0)
			status = 1;
	}

	return (status);
}
