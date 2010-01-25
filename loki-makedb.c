#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h> /* TODO: assertion must be enabled */
#include <zlib.h>
#include "loki.h"

struct mypage_struct {
	char *title;
	uint32_t title_hash;
	uint32_t block_num;
	uint32_t title_sp_offset;
	union {
		uint32_t block_offset;
		char *redirect;
	};
};

/* string pool. can't be freed */
#define SP_SIZE 65536
static char *sp_pool = NULL;
static int sp_len = 0;

static unsigned int min_block_size = 256 * 1024; /* minimal compressed block size */
static unsigned int min_file_size = 1024 * 1024 * 1024; /* minimal compressed file size */
static const char *language = NULL;

static FILE *outfile;
static z_stream zstream;
static unsigned char zoutbuf[8192];
static unsigned int file_num; /* current file id */
static unsigned int file_offset; /* current file pointer/offset */
static uint32_t block_num; /* current block id */
static uint32_t block_offset; /* uncompressed offset within block */
static struct loki_block *blocks;
static int block_cap;

static struct mypage_struct **pages;
static int page_num;
static int page_cap;

static uint32_t finger_bits;
static uint32_t *fingers;


static char *sp_alloc (int size)
{
	if (sp_pool == NULL) { /* first time running sp_dup */
		sp_pool = (char *)malloc(SP_SIZE);
		sp_len = SP_SIZE;
	}

	if (size > sp_len) {
		if (size > 1024)
			return (char *)malloc(size);
		sp_pool = (char *)malloc(SP_SIZE);
		sp_len = SP_SIZE;
	}

	sp_pool += size;
	sp_len -= size;
	return sp_pool - size;
}

// static char *sp_dup (char *ptr, int len)
// {
// 	char *newptr = sp_alloc(len);
// 	return memcpy(newptr, ptr, len);
// }

static char *sp_strdup (char *str)
{
	int len = strlen(str);
	char *newstr = sp_alloc(len + 1);
	return memcpy(newstr, str, len + 1);
}

/* read a line from STDIN.
 * BTW, why is it so difficult ro read a line in C? */
static struct mcs_struct *read_line (void)
{
	static struct mcs_struct *line = NULL;

	if (line == NULL) /* first time been called */
		line = mcs_create(8192);

	if (fgets(line->ptr, line->cap, stdin) == NULL)
		return NULL;
	line->len = strlen(line->ptr);
	if (line->len == 0) {
		printf("WTF!\n");
		return NULL;
	}

	while (line->ptr[line->len - 1] != '\n') {
		mcs_expand(line, 0);
		if (fgets(line->ptr + line->len, line->cap - line->len, stdin) == NULL)
			break;
		line->len = strlen(line->ptr);
	}

	return line;
}

static void out_dump (void)
{
	if (zstream.avail_out >= sizeof(zoutbuf))
		return;

	file_offset += sizeof(zoutbuf) - zstream.avail_out;
	if (fwrite(zoutbuf, sizeof(zoutbuf) - zstream.avail_out, 1, outfile) != 1) {
		printf("fwrite() failed vfbyuremcxkl\n");
		exit(1);
	}
	zstream.next_out = zoutbuf;
	zstream.avail_out = sizeof(zoutbuf);
}

static void out_init (void)
{
	char filename[256];

	assert(language != NULL);

	file_num = 0;
	file_offset = 0;
	snprintf(filename, sizeof(filename), "lokidb-%s.%02d.data", language, file_num);
	outfile = fopen(filename, "wb");
	if (outfile == NULL) {
		printf("error opening %s for writing\n", filename);
		exit(1);
	}

	block_num = 0;
	block_offset = 0;
	block_cap = 4096;
	blocks = (struct loki_block *)malloc(block_cap * sizeof(struct loki_block));

	memset(&zstream, 0, sizeof(zstream));
	assert(deflateInit(&zstream, Z_DEFAULT_COMPRESSION) == Z_OK);
	zstream.next_out = zoutbuf;
	zstream.avail_out = sizeof(zoutbuf);
}

static void out_write (char *data, int size,
		uint32_t *the_block_num, uint32_t *the_block_offset)
{
	/* should we start a new block? */
	if (file_offset - blocks[block_num].file_offset >= min_block_size) {
		/* close current block */
		out_dump();
		assert(deflate(&zstream, Z_FULL_FLUSH) == Z_OK);
		out_dump();
		blocks[block_num].size_zip = file_offset - blocks[block_num].file_offset;
		blocks[block_num].size_plain = block_offset;

		/* should we start a new file? */
		if (file_offset >= min_file_size) {
			char filename[256];
			fclose(outfile);
			file_num ++;
			snprintf(filename, sizeof(filename), "lokidb-%s.%02d.data", language, file_num);
			outfile = fopen(filename, "wb");
			assert(outfile != NULL);
			file_offset = 0;
		}

		block_num ++;
		block_offset = 0;
		if (block_num >= block_cap) {
			block_cap += block_cap;
			blocks = (struct loki_block *)realloc(blocks, block_cap * sizeof(struct loki_block));
		}
		blocks[block_num].file_num = file_num;
		blocks[block_num].file_offset = file_offset;
	}

	*the_block_num = block_num;
	*the_block_offset = block_offset;

	zstream.next_in = (unsigned char *)data;
	zstream.avail_in = size;
	do {
		if (zstream.avail_out == 0)
			out_dump();
		if (deflate(&zstream, Z_NO_FLUSH) != Z_OK) {
			printf("deflate() failed vnrueisog\n");
			exit(1);
		}
	} while (zstream.avail_in != 0);
}

static void out_fini (void)
{
	out_dump();
	assert(deflate(&zstream, Z_FINISH) == Z_STREAM_END);
	out_dump();
	deflateEnd(&zstream);
	blocks[block_num].size_zip = file_offset - blocks[block_num].file_offset;
	blocks[block_num].size_plain = block_offset;
	block_num ++;
	fclose(outfile);
}

static char *get_redirect (struct mcs_struct *text)
{
	return NULL;
}

static void add_page (struct mcs_struct *title, struct mcs_struct *text)
{
	char *redirect;
	struct mypage_struct *page;

	if (title->len == 0 || text->len == 0)
		return;

	page = (struct mypage_struct *)sp_alloc(sizeof(struct mypage_struct));
	page->title = sp_strdup(title->ptr);
	page->title_hash = loki_hash_title(title->ptr, title->len);
	if ((redirect = get_redirect(text))) {
		page->block_num = 0xffffffff;
		page->redirect = redirect;
	} else {
		out_write(text->ptr, text->len, &page->block_num, &page->block_offset);
	}

	if (page_num >= page_cap) {
		page_cap += page_cap;
		pages = (struct mypage_struct **)realloc(pages, page_cap * sizeof(struct mypage_struct *));
	}
	pages[page_num ++] = page;
}

static void load_data (void)
{
	struct mcs_struct *title = mcs_create(256);
	struct mcs_struct *text = mcs_create(65536);
	int inside_text = 0;

	while (1) {
		struct mcs_struct *line = read_line();

		if (line == NULL)
			break;
		if (line->len == 0)
			continue;

		if (inside_text) { /* inside text */
			if (strstr(line->ptr, "</text>") == NULL) {
				mcs_cat(text, line->ptr, line->len);
			} else {
				mcs_cat(text, line->ptr, strstr(line->ptr, "</text>") - line->ptr);
				add_page(title, text);
				inside_text = 0;
			}
		} else { /* outside text */
			if (memcmp(line->ptr, "    <title>", 11) == 0) {
				if (strstr(line->ptr, "</title>") == NULL) {
					printf("opps, line \"%s\" unexpected\n", line->ptr);
					exit(1);
				}
				mcs_reset(title);
				mcs_cat(title, line->ptr + 11, strstr(line->ptr, "</title>") - line->ptr - 11);
			} else if (memcmp(line->ptr, "      <text xml:space=\"preserve\">", 33) == 0) {
				mcs_reset(text);
				if (strstr(line->ptr, "</text>") != NULL) { /* one line text */
					mcs_cat(text, line->ptr + 33, strstr(line->ptr, "</text>") - line->ptr - 33);
					add_page(title, text);
				} else { /* multi-line text */
					mcs_cat(text, line->ptr + 33, line->len - 33);
					inside_text = 1;
				}
			}
		}
	}

	mcs_free(title);
	mcs_free(text);
}

static int compare_page (const void *p1, const void *p2)
{
	const struct mypage_struct *page1 = *(struct mypage_struct * const *)p1;
	const struct mypage_struct *page2 = *(struct mypage_struct * const *)p2;

	if (page1->title_hash < page2->title_hash)
		return -1;
	if (page1->title_hash > page2->title_hash)
		return 1;
	return strcmp(page1->title, page2->title);
}

static struct mypage_struct *search_page (char *title)
{
	uint32_t title_hash = loki_hash_title(title, -1);
	int i;

	assert(fingers != NULL);

	for (i = fingers[title_hash >> (32 - finger_bits)];
			pages[i]->title_hash <= title_hash;
			i ++) {
		if (pages[i]->title_hash == title_hash && strcmp(pages[i]->title, title) == 0)
			return pages[i];
	}
	return NULL;
}

static void gen_index (void)
{
	char filename[256];
	struct loki_sb superblock;
	FILE *indexfile;
	int i, title_sp_offset;

	assert(language != NULL);
	assert(strlen(language) <= 7);
	assert(block_num > 0 && page_num > 0);

	/* finger_bits should be about log2(page_num)/2 */
	for (finger_bits = 31; (page_num & (1 << finger_bits)) == 0; finger_bits --)
		;
	finger_bits /= 2;
	if (finger_bits < 1)
		finger_bits = 1;
	fingers = (uint32_t *)malloc((1 << finger_bits) * 4);

	snprintf(filename, sizeof(filename), "lokidb-%s.index", language);
	indexfile = fopen(filename, "wb");
	if (indexfile == NULL) {
		printf("failed to open %s for writing\n", filename);
		exit(1);
	}

	memset(&superblock, 0, sizeof(superblock));
	memcpy(superblock.magic, "LOKI\0IDX", 8);
	strcpy(superblock.lang, language);
	superblock.file_num = file_num;
	superblock.block_num = block_num;
	superblock.finger_bits = finger_bits;
	superblock.page_num = page_num;
	superblock.min_block_size = min_block_size;
	superblock.min_file_size = min_file_size;
	assert(fwrite(&superblock, sizeof(superblock), 1, indexfile) == 1);

	assert(fwrite(blocks, sizeof(struct loki_block) * block_num, 1, indexfile) == 1);

	/* sort pages by hashval */
	qsort(pages, page_num, sizeof(struct mypage_struct *), compare_page);

	/* warn duplicate pages */
	for (i = 1; i < page_num; i ++) {
		if (pages[i-1]->title_hash == pages[i]->title_hash &&
				strcmp(pages[i-1]->title, pages[i]->title) == 0)
			printf("Warning: duplicate page \"%s\"\n", pages[i]->title);
	}

	/* setup finger table */
	for (i = 0; i < 1 << finger_bits; i ++)
		fingers[i] = page_num;
	for (i = page_num - 1; i >= 0; i --)
		fingers[pages[i]->title_hash >> (32 - finger_bits)] = i;
	assert(fwrite(fingers, (1 << finger_bits) * 4, 1, indexfile) == 1);

	/* 1. get title_sp_offset */
	title_sp_offset = 0;
	for (i = 0; i < page_num; i ++) {
		pages[i]->title_sp_offset = title_sp_offset;
		title_sp_offset += strlen(pages[i]->title) + 1;
	}
	/* 2. write loki_page_entry */
	for (i = 0; i < page_num; i ++) {
		struct loki_page_entry page_entry;

		page_entry.title_hash = pages[i]->title_hash;
		page_entry.title_offset = pages[i]->title_sp_offset;
		page_entry.block_num = pages[i]->block_num;
		if (pages[i]->block_num == 0xffffffff) {
			struct mypage_struct *another_page = search_page(pages[i]->redirect);
			if (another_page) {
				page_entry.block_offset = another_page->title_sp_offset;
			} else {
				page_entry.block_offset = title_sp_offset;
				title_sp_offset += strlen(pages[i]->redirect) + 1;
			}
		} else {
			page_entry.block_offset = pages[i]->block_offset;
		}
		assert(fwrite(&page_entry, sizeof(page_entry), 1, indexfile) == 1);
	}
	/* 3. write string pool */
	for (i = 0; i < page_num; i ++) {
		assert(fwrite(pages[i]->title, strlen(pages[i]->title) + 1, 1, indexfile) == 1);
	}
	for (i = 0; i < page_num; i ++) {
		if (!search_page(pages[i]->redirect))
			assert(fwrite(pages[i]->redirect,
						strlen(pages[i]->redirect) + 1,
						1, indexfile) == 1);
	}
	fclose(indexfile);
}

static void usage (void)
{
}

int main (int argc, char *argv[])
{
	int i;

	for (i = 1; i < argc; i ++) {
		if (strcmp(argv[i], "-h") == 0 ||
				strcmp(argv[i], "--help") == 0 ||
				strcmp(argv[i], "-?") == 0) {
			usage();
			return 0;
		} else if (strcmp(argv[i], "-b") == 0) {
			min_block_size = atoi(argv[++i]);
			if (min_block_size < 1000 || min_block_size > 10000000) {
				printf("invalid min_block_size. it should be in [1000,10000000]\n");
				return 1;
			}
		} else if (strcmp(argv[i], "-f") == 0) {
			min_file_size = atoi(argv[++i]);
			if (min_file_size < 64000000) {
				printf("min_file_size too small. it should be at least 64000000\n");
				return 1;
			}
		} else if (argv[i][0] == '-') {
			printf("unknown option %s\n", argv[i]);
			return 1;
		} else {
			if (language != NULL) {
				printf("wrong command line.\n");
				return 1;
			}
			language = argv[i];
		}
	}
	if (language == NULL) {
		printf("no language specified.\n");
		usage();
		return 1;
	}
	if (strlen(language) > 7) {
		printf("wrong language\n");
		return 1;
	}

	page_cap = 1024 * 1024;
	pages = malloc(page_cap * sizeof(struct mypage_struct *));
	page_num = 0;

	out_init();
	load_data();
	out_fini();
	if (page_num == 0) {
		printf("error: no page loaded\n");
		return 1;
	}
	gen_index();

	return 0;
}
