#include <stdlib.h>
#include <string.h>
#include "lxplib.h"

#define ISHEX(c) (((c) >= '0' && (c) <= '9') || \
	((c) >= 'A' && (c) <= 'Z') || \
	((c) >= 'a' && (c) <= 'z'))
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

/* decode things like %20 */
int decode_url_encoding (char *str)
{
	char *ptr;
	int len, count = 0;

restart:
	for (len = strlen(str), ptr = str; *ptr; ptr ++) {
		if (*ptr == '%' && ISHEX(*(ptr+1)) && ISHEX(*(ptr+2))) {
			*ptr = HEXVAL(*(ptr+1)) * 16 + HEXVAL(*(ptr+2));
			memmove(ptr + 1, ptr + 3, len - (ptr - str + 3) + 1);
			count ++;
			goto restart;
		}
	}
	return count;
}

/* decode things like &eacute; &#257; or &#x3017; */
int decode_html_entity (char *str)
{
	static const char *entities[] = {
		"&#34;", "&quot;", "\"",
		"&#38;", "&amp;", "&",
		"&#39;", "&apos;", "'",
		"&#60;", "&lt;", "<",
		"&#62;", "&gt;", ">",
		NULL, NULL, NULL}; //TODO: add more
	int i, count = 0;

restart:
	for (i = 0; entities[i]; i ++) {
		char *ptr;
		if (i % 3 == 2)
			continue;
		ptr = strstr(str, entities[i]);
		if (!ptr)
			continue;
		memmove(ptr + strlen(entities[i+2-i%3]),
				ptr + strlen(entities[i]),
				strlen(ptr + strlen(entities[i])) + 1);
		memcpy(ptr, entities[i+2-i%3], strlen(entities[i+2-i%3]));
		count ++;
		goto restart;
	}
	return count;
}

int decode_url_html (char *str)
{
	int sum = 0, retval;
	do {
		retval = decode_url_encoding(str);
		sum += retval;
		retval = decode_html_entity(str);
		sum += retval;
	} while (retval);
	return sum;
}
