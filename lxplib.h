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
	/* block_num:
	 * 0-0xffffffff: normal page with full text
	 * 0xfffffffe: redirect with anchor.
	 *   block_offset is offset into string pool containing:
	 *   4-byte page_id of the redirected page,
	 *   followed by the anchor name.
	 * 0xffffffff: redirect with no anchor.
	 *   block_offset is page_id of the redirected page.
	 */
	uint32_t block_num;
	uint32_t block_offset;
} PACK_STRUCT ;

char *mystrncpy (char *dest, const char *src, size_t n);
struct mcs_struct *mcs_create (int initsize);
void mcs_reset (struct mcs_struct *mcs);
void mcs_expand (struct mcs_struct *mcs, int newsize);
void mcs_cat (struct mcs_struct *dst, char *src, int src_len);
void mcs_free (struct mcs_struct *mcs);

uint32_t lxp_hash_title (char *title, int length);

extern const unsigned short cmap_toupper[65536];
extern const unsigned short cmap_tolower[65536];
#define uni_toupper(c) (cmap_toupper[c])
#define uni_tolower(c) (cmap_tolower[c])

int decode_utf8 (const char *instr, unsigned int *punival);
int encode_utf8 (char *outstr, unsigned int unival);
void first_toupper (char *str);
int decode_url_encoding (char *str);
int decode_html_entity_minimal (char *str);
int decode_html_entity_full (char *str);

int parse_title_from_text (const char *_text, int text_len,
		char *title, int title_size,
		char *anchor, int anchor_size,
		char *interwiki, int interwiki_size,
		char *namespace, int namespace_size,
		char *label, int label_size);
int parse_title_from_text_keepns (const char *text, int text_len,
		char *title, int title_size,
		char *anchor, int anchor_size,
		char *interwiki, int interwiki_size,
		char *label, int label_size);
int parse_title_from_redirect_keepns (const char *_text, int text_len,
		char *title, int title_size,
		char *anchor, int anchor_size,
		char *interwiki, int interwiki_size,
		char *label, int label_size);

#endif
