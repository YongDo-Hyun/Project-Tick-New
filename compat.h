#ifndef ED_COMPAT_H
#define ED_COMPAT_H

#include <stddef.h>

size_t ed_strlcpy(char *dst, const char *src, size_t dstsize);

#define strlcpy ed_strlcpy

#endif
