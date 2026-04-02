/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026
 *  Project Tick. All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "compat.h"

#include <string.h>

size_t
pax_strlcpy(char *dst, const char *src, size_t dsize)
{
	const char *osrc;
	size_t nleft;

	osrc = src;
	nleft = dsize;
	if (nleft != 0) {
		while (--nleft != 0) {
			if ((*dst++ = *src++) == '\0')
				break;
		}
	}

	if (nleft == 0) {
		if (dsize != 0)
			*dst = '\0';
		while (*src++ != '\0')
			;
	}

	return ((size_t)(src - osrc - 1));
}

void
pax_strmode(mode_t mode, char *buf)
{
	switch (mode & S_IFMT) {
	case S_IFDIR:
		*buf++ = 'd';
		break;
	case S_IFCHR:
		*buf++ = 'c';
		break;
	case S_IFBLK:
		*buf++ = 'b';
		break;
	case S_IFREG:
		*buf++ = '-';
		break;
	case S_IFLNK:
		*buf++ = 'l';
		break;
	case S_IFSOCK:
		*buf++ = 's';
		break;
#ifdef S_IFIFO
	case S_IFIFO:
		*buf++ = 'p';
		break;
#endif
	default:
		*buf++ = '?';
		break;
	}

	*buf++ = (mode & S_IRUSR) ? 'r' : '-';
	*buf++ = (mode & S_IWUSR) ? 'w' : '-';
	switch (mode & (S_IXUSR | S_ISUID)) {
	case 0:
		*buf++ = '-';
		break;
	case S_IXUSR:
		*buf++ = 'x';
		break;
	case S_ISUID:
		*buf++ = 'S';
		break;
	default:
		*buf++ = 's';
		break;
	}

	*buf++ = (mode & S_IRGRP) ? 'r' : '-';
	*buf++ = (mode & S_IWGRP) ? 'w' : '-';
	switch (mode & (S_IXGRP | S_ISGID)) {
	case 0:
		*buf++ = '-';
		break;
	case S_IXGRP:
		*buf++ = 'x';
		break;
	case S_ISGID:
		*buf++ = 'S';
		break;
	default:
		*buf++ = 's';
		break;
	}

	*buf++ = (mode & S_IROTH) ? 'r' : '-';
	*buf++ = (mode & S_IWOTH) ? 'w' : '-';
	switch (mode & (S_IXOTH | S_ISVTX)) {
	case 0:
		*buf++ = '-';
		break;
	case S_IXOTH:
		*buf++ = 'x';
		break;
	case S_ISVTX:
		*buf++ = 'T';
		break;
	default:
		*buf++ = 't';
		break;
	}

	*buf++ = ' ';
	*buf = '\0';
}
