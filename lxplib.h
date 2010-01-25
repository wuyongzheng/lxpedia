#ifndef LXPLIB_H
#define LXPLIB_H

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

struct lxp_sb {
	uint8_t magic[8];
	char lang[8]; /* e.g. en, zh. NULL padded. */
	uint32_t file_num;
	uint32_t block_num;
	/* finger_bits: how many high bits of title_hash is used in finger table
	 * finger_num = 2^finger_bits */
	uint32_t finger_bits;
	uint32_t page_num;
	uint32_t min_block_size;
	uint32_t min_file_size;
} PACK_STRUCT ;

struct lxp_block {
	uint32_t file_num;
	uint32_t file_offset;
	uint32_t size_plain;
	uint32_t size_zip;
} PACK_STRUCT ;

struct lxp_page_entry {
	uint32_t title_hash;
	uint32_t title_offset;
	uint32_t block_num; /* if it's 0xffffffff, it's a redirect */
	uint32_t block_offset; /* for redirect, it's redirect title */
} PACK_STRUCT ;

struct mcs_struct *mcs_create (int initsize);
void mcs_reset (struct mcs_struct *mcs);
void mcs_expand (struct mcs_struct *mcs, int newsize);
void mcs_cat (struct mcs_struct *dst, char *src, int src_len);
void mcs_free (struct mcs_struct *mcs);

uint32_t lxp_hash_title (char *title, int length);

#endif
