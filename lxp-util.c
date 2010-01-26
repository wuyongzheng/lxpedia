#include <stdlib.h>
#include <string.h>
#include "lxplib.h"

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
