#ifndef LOKI_H
#define LOKI_H

#include <stdint.h>

#ifdef GT_POSIX
# define PACK_STRUCT __attribute__((packed))
#else
# define PACK_STRUCT
# pragma pack(push, 1)
#endif

#define MAX_TITLE_LEN 255

struct mcs_struct {
	char *ptr;
	int len;
	int cap;
};

struct loki_sb {
	uint8_t magic[8];
	char lang[8]; /* e.g. en, zh. NULL padded. */
	uint32_t finger_bits;
	uint32_t page_num;
} PACK_STRUCT ;

struct loki_hentry {
	uint32_t hashval;
	uint32_t title_offset;
	uint32_t page_block;
	uint32_t page_offset;
} PACK_STRUCT ;

struct mcs_struct *mcs_create (int initsize);
void mcs_reset (struct mcs_struct *mcs);
void mcs_expand (struct mcs_struct *mcs, int newsize);
void mcs_cat (struct mcs_struct *dst, char *src, int src_len);

#endif
