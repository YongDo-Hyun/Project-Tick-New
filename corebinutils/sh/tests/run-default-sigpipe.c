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

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
reset_sigpipe(void)
{
	struct sigaction sa;
	sigset_t set;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGPIPE, &sa, NULL) != 0) {
		fprintf(stderr, "run-default-sigpipe: sigaction(SIGPIPE): %s\n",
		    strerror(errno));
		exit(125);
	}

	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) != 0) {
		fprintf(stderr, "run-default-sigpipe: sigprocmask(SIGPIPE): %s\n",
		    strerror(errno));
		exit(125);
	}
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s program [args...]\n", argv[0]);
		return (125);
	}

	reset_sigpipe();
	execv(argv[1], argv + 1);

	fprintf(stderr, "run-default-sigpipe: execv(%s): %s\n", argv[1],
	    strerror(errno));
	return (125);
}
