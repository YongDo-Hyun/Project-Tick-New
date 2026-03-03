#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/auxv.h>
#endif

#include "compat.h"

int
sh_issetugid(void)
{
#if defined(__linux__) && defined(AT_SECURE)
	errno = 0;
	if (getauxval(AT_SECURE) != 0)
		return (1);
	if (errno == 0)
		return (0);
#endif
	return (getuid() != geteuid() || getgid() != getegid());
}

size_t
sh_strlcpy(char *dst, const char *src, size_t dstsize)
{
	size_t srclen;

	srclen = strlen(src);
	if (dstsize != 0) {
		size_t copylen;

		copylen = srclen >= dstsize ? dstsize - 1 : srclen;
		memcpy(dst, src, copylen);
		dst[copylen] = '\0';
	}
	return (srclen);
}
