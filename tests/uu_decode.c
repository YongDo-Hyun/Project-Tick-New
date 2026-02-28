/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
decode_char(int c)
{
	return (c - 32) & 0x3f;
}

static int
decode_line(const char *line, FILE *out)
{
	int length;
	int produced;

	length = decode_char((unsigned char)line[0]);
	if (length == 0)
		return 0;

	line++;
	produced = 0;
	while (produced < length) {
		unsigned char a;
		unsigned char b;
		unsigned char c;
		unsigned char d;
		unsigned char bytes[3];
		int chunk;

		if (line[0] == '\0' || line[1] == '\0' || line[2] == '\0' ||
		    line[3] == '\0') {
			fprintf(stderr, "uu_decode: truncated uuencoded line\n");
			return -1;
		}

		a = (unsigned char)decode_char((unsigned char)line[0]);
		b = (unsigned char)decode_char((unsigned char)line[1]);
		c = (unsigned char)decode_char((unsigned char)line[2]);
		d = (unsigned char)decode_char((unsigned char)line[3]);
		line += 4;

		bytes[0] = (unsigned char)((a << 2) | (b >> 4));
		bytes[1] = (unsigned char)((b << 4) | (c >> 2));
		bytes[2] = (unsigned char)((c << 6) | d);

		chunk = length - produced;
		if (chunk > 3)
			chunk = 3;
		if (fwrite(bytes, 1, (size_t)chunk, out) != (size_t)chunk) {
			fprintf(stderr, "uu_decode: write failed: %s\n",
			    strerror(errno));
			return -1;
		}
		produced += chunk;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	FILE *in;
	FILE *out;
	char line[4096];
	int seen_begin;

	if (argc != 3) {
		fprintf(stderr, "usage: %s input.uu output\n", argv[0]);
		return 1;
	}

	in = fopen(argv[1], "r");
	if (in == NULL) {
		fprintf(stderr, "uu_decode: cannot open %s: %s\n", argv[1],
		    strerror(errno));
		return 1;
	}
	out = fopen(argv[2], "wb");
	if (out == NULL) {
		fprintf(stderr, "uu_decode: cannot open %s: %s\n", argv[2],
		    strerror(errno));
		fclose(in);
		return 1;
	}

	seen_begin = 0;
	while (fgets(line, sizeof(line), in) != NULL) {
		size_t len;

		len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		if (!seen_begin) {
			if (strncmp(line, "begin ", 6) == 0)
				seen_begin = 1;
			continue;
		}
		if (strcmp(line, "end") == 0)
			break;
		if (decode_line(line, out) != 0) {
			fclose(out);
			fclose(in);
			return 1;
		}
	}

	if (fclose(out) != 0) {
		fprintf(stderr, "uu_decode: cannot close %s: %s\n", argv[2],
		    strerror(errno));
		fclose(in);
		return 1;
	}
	if (fclose(in) != 0) {
		fprintf(stderr, "uu_decode: cannot close %s: %s\n", argv[1],
		    strerror(errno));
		return 1;
	}
	if (!seen_begin) {
		fprintf(stderr, "uu_decode: missing begin line in %s\n", argv[1]);
		return 1;
	}

	return 0;
}
