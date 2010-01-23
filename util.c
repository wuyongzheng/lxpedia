#include <stdlib.h>
#include <string.h>
#include "loki.h"

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
	if (mcs->cap >= newsize)
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
