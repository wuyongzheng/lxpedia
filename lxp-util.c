#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lxplib.h"

#define ISHEX(c) (((c) >= '0' && (c) <= '9') || \
	((c) >= 'A' && (c) <= 'f') || \
	((c) >= 'a' && (c) <= 'f'))
#define HEXVAL(c) ((unsigned char)(c) >= 'a' ? \
	(unsigned char)(c)-'a'+10 : \
	(unsigned char)(c) >= 'A' ? \
		(unsigned char)(c)-'A'+10 : \
		(unsigned char)(c)-'0')

struct mcs_struct *mcs_create (int initsize)
{
	struct mcs_struct *mcs = (struct mcs_struct *)malloc(sizeof(struct mcs_struct));
	mcs->ptr = (char *)malloc(initsize);
	mcs->cap = initsize;
	mcs->len = 0;
	mcs->ptr[0] = '\0';
	return mcs;
}

void mcs_reset (struct mcs_struct *mcs)
{
	mcs->len = 0;
	mcs->ptr[0] = '\0';
}

void mcs_expand (struct mcs_struct *mcs, int newsize)
{
	if (newsize > 0 && newsize <= mcs->cap)
		return;

	if (newsize <= 0)
		mcs->cap += mcs->cap;
	else
		mcs->cap = newsize;
	mcs->ptr = realloc(mcs->ptr, mcs->cap);
}

void mcs_cat (struct mcs_struct *dst, char *src, int src_len)
{
	if (dst->len + src_len > dst->cap) {
		mcs_expand(dst, dst->len + src_len > dst->cap * 2 ?
				dst->len + src_len : dst->cap * 2);
	}
	memcpy(dst->ptr + dst->len, src, src_len);
	dst->len += src_len;
	dst->ptr[dst->len] = '\0';
}

void mcs_free (struct mcs_struct *mcs)
{
	if (mcs == NULL)
		return;
	free(mcs->ptr);
	free(mcs);
}

uint32_t lxp_hash_title (char *title, int length)
{
	uint32_t hashval = 0;
	if (length < 0) {
		for (; *title; title ++)
			hashval = hashval * 31 + *(unsigned char *)title;
	} else {
		for (; length > 0; length --, title ++)
			hashval = hashval * 31 + *(unsigned char *)title;
	}
	return hashval;
}

/* given a unicode character "unival", generate the UTF-8
 * string in "outstr", return the length of it.
 * The output string is NOT null terminated. */
int encode_utf8 (char *outstr, unsigned int unival)
{
	if (unival < 0x80) {
		outstr[0] = unival;
		return 1;
	}
	if (unival < 0x800) {
		outstr[0] = 0xc0 | (unival >> 6);
		outstr[1] = 0x80 | (unival & 0x3f);
		return 2;
	}
	if (unival < 0x10000) {
		outstr[0] = 0xe0 | (unival >> 12);
		outstr[1] = 0x80 | ((unival >> 6) & 0x3f);
		outstr[2] = 0x80 | (unival & 0x3f);
		return 3;
	}
	if (unival < 0x110000) {
		outstr[0] = 0xf0 | (unival >> 18);
		outstr[1] = 0x80 | ((unival >> 12) & 0x3f);
		outstr[2] = 0x80 | ((unival >> 6) & 0x3f);
		outstr[3] = 0x80 | (unival & 0x3f);
		return 4;
	}
	assert(0);
}

/* decode things like %20 */
int decode_url_encoding (char *str)
{
	char *inptr, *outptr;
	int count;

	inptr = strchr(str, '%');
	if (!inptr)
		return 0;

	outptr = inptr;
	count = 0;
	while (*inptr) {
		if (*inptr == '%' && ISHEX(inptr[1]) && ISHEX(inptr[2])) {
			*(outptr++) = HEXVAL(inptr[1]) * 16 + HEXVAL(inptr[2]);
			inptr += 3;
			count ++;
			continue;
		}
		*(outptr++) = *(inptr++);
	}
	*outptr = '\0';
	return count;
}

/* only decode the must-escape ones: &quot; &amp; &apos; &lt; &gt;
 * it's used to extract text from wiki xml dump */
int decode_html_entity_minimal (char *str)
{
	char *inptr, *outptr;
	int count;

	inptr = strchr(str, '&');
	if (!inptr)
		return 0;

	outptr = inptr;
	count = 0;
	while (*inptr) {
		if (*inptr != '&')
			goto nomatch;
		if (memcmp(inptr, "&quot;", 6) == 0) {*(outptr++) = '"';  inptr += 6; count++; continue;}
		if (memcmp(inptr, "&amp;",  5) == 0) {*(outptr++) = '&';  inptr += 5; count++; continue;}
		if (memcmp(inptr, "&apos;", 6) == 0) {*(outptr++) = '\''; inptr += 6; count++; continue;}
		if (memcmp(inptr, "&lt;",   4) == 0) {*(outptr++) = '<';  inptr += 4; count++; continue;}
		if (memcmp(inptr, "&gt;",   4) == 0) {*(outptr++) = '>';  inptr += 4; count++; continue;}
nomatch:
		*(outptr++) = *(inptr++);
	}
	*outptr = '\0';
	return count;
}

struct entity_struct {
	int name_len;
	int value_len;
	const char *name;
	const char *value;
};
/* They ought to be sorted by probability, higher probability goes in front. */
const struct entity_struct entities_full[] = {
	{6, 1, "&quot;", "\""},
	{5, 1, "&amp;", "&"},
	{6, 1, "&apos;", "'"},
	{4, 1, "&lt;", "<"},
	{4, 1, "&gt;", ">"},
	{0, 0, NULL, NULL}
};

/* decode things like &eacute; &#257; or &#x3017; */
int decode_html_entity_full (char *str)
{
	char *inptr, *outptr;
	int count;

	inptr = strchr(str, '&');
	if (!inptr)
		return 0;

	outptr = inptr;
	count = 0;
	while(*inptr) {
		const struct entity_struct *entity;
		const char *replace;
		char replace_buff[5];
		int match_len, replace_len;

		if (*inptr != '&')
			goto nomatch;
		for (entity = entities_full; entity->name_len; entity ++) {
			if (memcmp(inptr, entity->name, entity->name_len) == 0) {
				replace = entity->value;
				match_len = entity->name_len;
				replace_len = entity->value_len;
				goto match;
			}
		}
		if (*(inptr+1) != '#')
			goto nomatch;
		if (*(inptr+2) == 'x') { /* &#x3017; */
			int digits, unival;
			for (digits = 0, unival = 0; digits < 7 && ISHEX(*(inptr+3+digits)); digits ++)
				unival = unival * 16 + HEXVAL(*(inptr+3+digits));
			if (digits == 0 || *(inptr+3+digits) != ';')
				goto nomatch;
			replace_len = encode_utf8(replace_buff, unival);
			replace = replace_buff;
			match_len = digits + 4;
		} else { /* &#257; */
			int digits, unival;
			for (digits = 0, unival = 0; digits < 8 && *(inptr+2+digits) >= '0' &&
					*(inptr+2+digits) <= '9'; digits ++)
				unival = unival * 10 + *(inptr+2+digits) - '0';
			if (digits == 0 || *(inptr+2+digits) != ';')
				goto nomatch;
			replace_len = encode_utf8(replace_buff, unival);
			replace = replace_buff;
			match_len = digits + 3;
		}

match:
		memcpy(outptr, replace, replace_len);
		inptr += match_len;
		outptr += replace_len;
		count ++;
		continue;

nomatch:
		*(outptr++) = *(inptr++);
	}
	*outptr = '\0';
	return count;
}
