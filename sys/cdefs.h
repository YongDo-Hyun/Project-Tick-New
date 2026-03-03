#ifndef SH_SYS_CDEFS_H
#define SH_SYS_CDEFS_H

#if defined(__GLIBC__)
#include_next <sys/cdefs.h>
#else
#include "../cdefs.h"
#endif

#endif
