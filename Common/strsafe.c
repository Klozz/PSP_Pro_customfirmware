/*
 * This file is part of PRO CFW.

 * PRO CFW is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * PRO CFW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PRO CFW. If not, see <http://www.gnu.org/licenses/ .
 */

#include <pspsdk.h>
#include <pspiofilemgr.h>
#include <psploadcore.h>
#include <psppower.h>
#include <stdio.h>
#include <string.h>
#include "printk.h"
#include "strsafe.h"

extern size_t strnlen(const char *s, size_t len);

char * strncat(char *dst, const char *src, size_t n)
{
	if (n != 0) {
		char *d = dst;
		const char *s = src;

		while (*d != 0)
			d++;
		do {
			if ((*d = *s++) == 0)
				break;
			d++;
		} while (--n != 0);
		*d = 0;
	}
	return (dst);
}

size_t strncat_s(char *strDest, size_t numberOfElements, const char *strSource, size_t count)
{
	size_t rest;

	if (!strDest || !strSource || numberOfElements == 0) {
		return 0;
	}

	rest = numberOfElements - strnlen(strDest, numberOfElements);

	if (rest == 0) {
		return 0;
	}

	rest--;
	strncat(strDest, strSource, rest < count ? rest : count);

	return strnlen(strDest, numberOfElements);
}

size_t strncpy_s(char *strDest, size_t numberOfElements, const char *strSource, size_t count)
{
	if (!strDest || !strSource || numberOfElements == 0) {
		return 0;
	}

	strncpy(strDest, strSource, numberOfElements < count ?
			numberOfElements : count);
	strDest[numberOfElements - 1] = '\0';

	return strnlen(strDest, numberOfElements);
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
	const unsigned char *p1 = (const unsigned char *) s1;
	const unsigned char *p2 = (const unsigned char *) s2;
	unsigned char c1, c2;

	if (p1 == p2 || n == 0)
		return 0;

	do {
		c1 = tolower(*p1);
		c2 = tolower(*p2);

		if (--n == 0 || c1 == '\0')
			break;

		++p1;
		++p2;
	} while (c1 == c2);

	return c1 - c2;
}

int strcasecmp(const char *s1, const char *s2)
{
	return strncasecmp(s1, s2, (u32)-1);
}
