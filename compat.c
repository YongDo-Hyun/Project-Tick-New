/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "compat.h"

size_t
ed_strlcpy(char *dst, const char *src, size_t dstsize)
{
	const char *s;
	size_t srclen;

	s = src;
	while (*s != '\0')
		s++;
	srclen = (size_t)(s - src);

	if (dstsize != 0) {
		size_t copylen;
		size_t i;

		copylen = srclen;
		if (copylen >= dstsize)
			copylen = dstsize - 1;
		for (i = 0; i < copylen; i++)
			dst[i] = src[i];
		dst[copylen] = '\0';
	}

	return srclen;
}
