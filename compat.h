/*

SPDX-License-Identifier: BSD-3-Clause

Copyright (c) 2026
 Project Tick. All rights reserved.

This code is derived from software contributed to Berkeley by
the Institute of Electrical and Electronics Engineers, Inc.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the University nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

*/

#ifndef SH_COMPAT_H
#define SH_COMPAT_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <time.h>

#ifndef __DECONST
#define __DECONST(type, var) ((type)(uintptr_t)(const void *)(var))
#endif

#ifndef nitems
#define nitems(array) (sizeof(array) / sizeof((array)[0]))
#endif

#ifndef ALIGNBYTES
#define ALIGNBYTES (sizeof(uintptr_t) - 1)
#endif

#ifndef ALIGN
#define ALIGN(value) (((value) + ALIGNBYTES) & ~ALIGNBYTES)
#endif

#ifndef _PATH_STDPATH
#define _PATH_STDPATH "/usr/bin:/bin"
#endif

#ifndef MAXLOGNAME
#ifdef LOGIN_NAME_MAX
#define MAXLOGNAME (LOGIN_NAME_MAX + 1)
#else
#define MAXLOGNAME 256
#endif
#endif

#ifndef CLOCK_UPTIME
#define CLOCK_UPTIME CLOCK_MONOTONIC
#endif

#ifndef timespecsub
#define timespecsub(a, b, result) do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
	if ((result)->tv_nsec < 0) { \
		(result)->tv_sec--; \
		(result)->tv_nsec += 1000000000L; \
	} \
} while (0)
#endif

#ifndef timespeccmp
#define timespeccmp(a, b, cmp) \
	(((a)->tv_sec == (b)->tv_sec) ? \
	 ((a)->tv_nsec cmp (b)->tv_nsec) : \
	 ((a)->tv_sec cmp (b)->tv_sec))
#endif

int sh_issetugid(void);
size_t sh_strlcpy(char *dst, const char *src, size_t dstsize);

#define issetugid sh_issetugid
#define strlcpy sh_strlcpy

#endif
