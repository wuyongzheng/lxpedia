#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h> /* TODO: assertion must be enabled */
#include <regex.h>
#include <zlib.h>
#include "lxplib.h"

struct mypage_struct {
	char *title;
	uint32_t title_hash;
	uint32_t block_num;
	uint32_t title_offset; /* also used to mark deletion */
	union {
		uint32_t block_offset;
		char *redirect;
	};
};

/* string pool. can't be freed */
#define SP_SIZE 65536
static char *sp_pool = NULL;
static int sp_len = 0;

static unsigned int min_block_size = 64 * 1024; /* minimal compressed block size */
static unsigned int min_file_size = 1024 * 1024 * 1024; /* minimal compressed file size */
static int gen_listing = 0;
static const char *language = NULL;

static FILE *outfile;
static z_stream zstream;
static unsigned char zoutbuf[8192];
static unsigned int file_num; /* current file id */
static unsigned int file_offset; /* current file pointer/offset */
static uint32_t block_num; /* current block id */
static uint32_t block_offset; /* uncompressed offset within block */
static struct lxp_block *blocks;
static int block_cap;

static struct mypage_struct **pages;
static int page_num;
static int page_cap;

static uint32_t finger_bits;
static uint32_t *fingers;


static char *sp_alloc (int size)
{
	if (sp_pool == NULL) { /* first time running sp_alloc */
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
	assert(line->len > 0);

	while (line->ptr[line->len - 1] != '\n') {
		if (line->len + 10 > line->cap)
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
	snprintf(filename, sizeof(filename), "lxpdb-%s.%02d.data", language, file_num);
	outfile = fopen(filename, "wb");
	if (outfile == NULL) {
		printf("error opening %s for writing\n", filename);
		exit(1);
	}

	block_num = 0;
	block_offset = 0;
	block_cap = 4096;
	blocks = (struct lxp_block *)malloc(block_cap * sizeof(struct lxp_block));

	memset(&zstream, 0, sizeof(zstream));
	assert(deflateInit(&zstream, Z_DEFAULT_COMPRESSION) == Z_OK);
	zstream.next_out = zoutbuf;
	zstream.avail_out = sizeof(zoutbuf);
}

static void out_write_data (unsigned char *data, int size)
{
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
	block_offset += size;
}

static void out_write (char *title, int title_len, char *text, int text_len,
		uint32_t *the_block_num, uint32_t *the_block_offset)
{
	/* should we start a new block? */
	if (file_offset - blocks[block_num].file_offset >= min_block_size) {
		/* close current block */
		assert(zstream.avail_in == 0);
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
			snprintf(filename, sizeof(filename), "lxpdb-%s.%02d.data", language, file_num);
			outfile = fopen(filename, "wb");
			assert(outfile != NULL);
			file_offset = 0;
		}

		block_num ++;
		block_offset = 0;
		if (block_num >= block_cap) {
			block_cap += block_cap;
			blocks = (struct lxp_block *)realloc(blocks, block_cap * sizeof(struct lxp_block));
		}
		blocks[block_num].file_num = file_num;
		blocks[block_num].file_offset = file_offset;
	}

	*the_block_num = block_num;
	*the_block_offset = block_offset;

	out_write_data((unsigned char *)title, title_len);
	out_write_data((unsigned char *)"\n", 1);
	out_write_data((unsigned char *)text, text_len);
	out_write_data((unsigned char *)"\0", 1);
}

static void out_fini (void)
{
	int retval;

	assert(zstream.avail_in == 0);
	out_dump();
	do {
		retval = deflate(&zstream, Z_FINISH);
		out_dump();
	} while (retval == Z_OK);
	assert(retval == Z_STREAM_END);
	deflateEnd(&zstream);
	blocks[block_num].size_zip = file_offset - blocks[block_num].file_offset;
	blocks[block_num].size_plain = block_offset;
	block_num ++;
	fclose(outfile);
}

/* referred to Title.php: newFromRedirectInternal()
 *   trim newline and spaces
 *   match #REDIRECT[[XXX]] and extract XXX. also remove "|" and after.
 *   urldecode()
 *   decodeCharReferences(): Convert things like &eacute; &#257; or &#x3017; into real text...
 *   " " to "_"
 *   remove /\xE2\x80[\x8E\x8F\xAA-\xAE]/
 *   merge and trim "_" and " "
 *   if it's A:B and A is a namespace, trim B.
 *   extract A, B from A#B, trim both
 *   first character toupper
 *   " " to "_"
 **/
static char *get_redirect (struct mcs_struct *text)
{
	static const char *namespaces[] = {
		"User", "Wikipedia", "File", "MediaWiki", "Template", "Help", "Category", "Portal", "Book",
		"Talk", "User talk", "Wikipedia talk", "File talk", "MediaWiki talk",
		"Template talk", "Help talk", "Category talk", "Portal talk", "Book talk",
		"Special", "Media", NULL};
	static const char *namespace_aliases[] = {
		"WP", "Wikipedia",
		"Project", "Wikipedia",
		"WT", "Wikipedia talk",
		"Project talk", "Wikipedia talk",
		"Image", "File",
		"Image talk", "File talk",
		NULL, NULL};
	static regex_t reg;
	static int reg_init = 0;
	char line[512], *redirect;
	int line_len, retval, i;
	regmatch_t matches[2];

	line_len = text->len < sizeof(line) - 10 ? text->len : sizeof(line) - 10;
	memcpy(line, text->ptr, line_len);
	line[line_len] = '\0';

	/* convert tab, newline and underscore to space */
	for (redirect = line; *redirect; redirect ++) {
		if (*(unsigned char *)redirect < ' ' || *redirect == '_')
			*redirect = ' ';
		else if (*redirect == '[') // TODO i'm stuck with the regxp problem: why [\[\]]* doesn't work?
			*redirect = '<';
		else if (*redirect == ']')
			*redirect = '>';
	}

	if (!reg_init) {
		assert(regcomp(&reg, "^ *#redirect *:? *<<[ :]*([^<>]+)>>", REG_ICASE | REG_EXTENDED) == 0);
		reg_init = 1;
	}
	retval = regexec(&reg, line, 2, matches, 0);
	if (retval == REG_NOMATCH)
		return NULL;
	assert(retval == 0);
	assert(matches[1].rm_so >= 0);
	assert(matches[1].rm_eo > matches[1].rm_so);
	redirect = line + matches[1].rm_so;
	redirect[matches[1].rm_eo - matches[1].rm_so] = '\0';

	if (strchr(redirect, '|'))
		strchr(redirect, '|')[0] = '\0';

	while (!decode_url_encoding(redirect) && !decode_html_entity_full(redirect))
		;

#define REPLACE_ALL(search,replace) { \
		char *_ptr_ = strstr(redirect, search); \
		while (_ptr_) { \
			memmove(_ptr_ + strlen(replace), \
					_ptr_ + strlen(search), \
					strlen(_ptr_ + strlen(search)) + 1); \
			memcpy(_ptr_, replace, strlen(replace)); \
			_ptr_ = strstr(redirect, search); \
		} \
	}
	REPLACE_ALL("_", " ");
	REPLACE_ALL("\xE2\x80\x8E", "");
	REPLACE_ALL("\xE2\x80\x8F", "");
	REPLACE_ALL("\xE2\x80\xAA", "");
	REPLACE_ALL("\xE2\x80\xAB", "");
	REPLACE_ALL("\xE2\x80\xAC", "");
	REPLACE_ALL("\xE2\x80\xAD", "");
	REPLACE_ALL("\xE2\x80\xAE", "");
	REPLACE_ALL("  ", " ");
	if (redirect[strlen(redirect) - 1] == ' ')
		redirect[strlen(redirect) - 1] = '\0';
	if (redirect[0] == ' ')
		memmove(redirect, redirect + 1, strlen(redirect));

	for (i = 0; namespace_aliases[i]; i += 2) {
		const char *alias = namespace_aliases[i];
		const char *realns = namespace_aliases[i+1];
		int alias_len = strlen(alias);
		int real_len = strlen(realns);
		// TODO: space
		if (strncasecmp(redirect, alias, alias_len) != 0 || redirect[alias_len] != ':')
			continue;
		memmove(redirect + real_len, redirect + alias_len, strlen(redirect + alias_len) + 1);
		memcpy(redirect, realns, real_len);
	}
	for (i = 0; namespaces[i]; i ++) {
		const char *ns = namespaces[i];
		int ns_len = strlen(ns);
		if (strncasecmp(redirect, ns, ns_len) != 0)
			continue;
		if (redirect[ns_len] != ':' && (redirect[ns_len] != ' ' || redirect[ns_len] != ':'))
			continue;
		memcpy(redirect, ns, ns_len);
		if (redirect[ns_len] == ' ')
			memmove(redirect + ns_len,
					redirect + ns_len + 1,
					strlen(redirect + ns_len));
		if (redirect[ns_len + 1] == ' ')
			memmove(redirect + ns_len + 1,
					redirect + ns_len + 2,
					strlen(redirect + ns_len + 1));
		first_toupper(redirect + ns_len + 1);
	}
	//TODO: how about interwiki?

	REPLACE_ALL(" #", "#");
	REPLACE_ALL("# ", "#");

	first_toupper(redirect);

#undef REPLACE_ALL

	return sp_strdup(redirect);
}

static void add_page (struct mcs_struct *title, struct mcs_struct *text)
{
	char *redirect;
	struct mypage_struct *page;

	/* This is the place to filter pages */
	if (title->len == 0 || text->len == 0)
		return;

	decode_html_entity_minimal(title->ptr);
	title->len = strlen(title->ptr);
	decode_html_entity_minimal(text->ptr);
	text->len = strlen(text->ptr);

	page = (struct mypage_struct *)sp_alloc(sizeof(struct mypage_struct));
	memset(page, 0, sizeof(struct mypage_struct));
	page->title = sp_strdup(title->ptr);
	page->title_hash = lxp_hash_title(title->ptr, title->len);
	if ((redirect = get_redirect(text))) {
		page->block_num = 0xffffffff;
		page->redirect = redirect;
	} else {
		out_write(title->ptr, title->len, text->ptr, text->len,
				&page->block_num, &page->block_offset);
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

	while (1) {
		struct mcs_struct *line = read_line();

		if (line == NULL)
			break;
		if (line->len == 0)
			continue;

		if (text->len > 0) { /* inside text */
			assert(title->len > 0);
			if (strstr(line->ptr, "</text>") == NULL) {
				mcs_cat(text, line->ptr, line->len);
			} else {
				mcs_cat(text, line->ptr, strstr(line->ptr, "</text>") - line->ptr);
				add_page(title, text);
				mcs_reset(title);
				mcs_reset(text);
			}
		} else { /* outside text */
			if (memcmp(line->ptr, "    <title>", 11) == 0) {
				if (strstr(line->ptr, "</title>") == NULL) {
					printf("opps, line \"%s\" unexpected\n", line->ptr);
					exit(1);
				}
				mcs_reset(title); /* in case <text xml:space="preserve" /> */
				mcs_cat(title, line->ptr + 11, strstr(line->ptr, "</title>") - line->ptr - 11);
			} else if (memcmp(line->ptr, "      <text xml:space=\"preserve\">", 33) == 0) {
				assert(title->len > 0);
				if (strstr(line->ptr, "</text>") != NULL) { /* one line text */
					mcs_cat(text, line->ptr + 33, strstr(line->ptr, "</text>") - line->ptr - 33);
					add_page(title, text);
					mcs_reset(title);
					mcs_reset(text);
				} else { /* multi-line text */
					mcs_cat(text, line->ptr + 33, line->len - 33);
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

static int search_page (char *title)
{
	uint32_t title_hash = lxp_hash_title(title, -1);
	int low = 0, high = page_num - 1;
	while (low <= high) {
		int med = (low + high) / 2;
		int comp = 1;
		if (pages[med]->title_hash == title_hash && (comp = strcmp(pages[med]->title, title)) == 0)
			return med;
		if (pages[med]->title_hash < title_hash || comp < 0)
			low = med + 1;
		else
			high = med - 1;
	}
	return -1;
}

static int search_redirect (const char *redirect, int ttl)
{
	char title[256];
	int pageid;

	if (ttl <= 0)
		return -1;

	strncpy(title, redirect, sizeof(title));
	title[sizeof(title) - 1] = '\0';
	if (strchr(title, '#'))
		strchr(title, '#')[0] = '\0';

	pageid = search_page(title);
	if (pageid == -1 || pages[pageid]->title_offset == 0xffffffff)
		return -1;
	if (pages[pageid]->block_num != 0xffffffff)
		return pageid;
	return search_redirect(pages[pageid]->redirect, ttl - 1);
}

static void gen_index (void)
{
	char filename[256];
	struct lxp_sb superblock;
	FILE *indexfile;
	int i, j, tp_offset;

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

	snprintf(filename, sizeof(filename), "lxpdb-%s.index", language);
	indexfile = fopen(filename, "wb");
	if (indexfile == NULL) {
		printf("failed to open %s for writing\n", filename);
		exit(1);
	}

	memset(&superblock, 0, sizeof(superblock));
	memcpy(superblock.magic, "LXP\0IDX\0", 8);
	strcpy(superblock.lang, language);
	superblock.file_num = file_num;
	superblock.block_num = block_num;
	superblock.finger_bits = finger_bits;
	superblock.page_num = page_num;
	superblock.min_block_size = min_block_size;
	superblock.min_file_size = min_file_size;
	assert(fwrite(&superblock, sizeof(superblock), 1, indexfile) == 1);
	printf("super block size: %d\n", (int)sizeof(superblock));

	assert(fwrite(blocks, sizeof(struct lxp_block) * block_num, 1, indexfile) == 1);
	printf("block listing size: %d\n", (int)sizeof(struct lxp_block) * block_num);

	/* sort pages by hashval */
	qsort(pages, page_num, sizeof(struct mypage_struct *), compare_page);

	/* mark duplicate pages */
	for (i = 1; i < page_num; i ++) {
		if (pages[i-1]->title_hash == pages[i]->title_hash &&
				strcmp(pages[i-1]->title, pages[i]->title) == 0) {
			printf("duplicate page \"%s\"\n", pages[i-1]->title);
			pages[i]->title_offset = 0xffffffff;
		}
	}
	/* mark broken redirect */
	for (i = 0; i < page_num; i ++) {
		if (pages[i]->block_num == 0xffffffff &&
				pages[i]->title_offset != 0xffffffff) {
			int pageid = search_redirect(pages[i]->redirect, 5);
			if (pageid == -1) {
				pages[i]->title_offset = 0xffffffff;
				printf("broken redirect \"%s\" -> \"%s\"\n", pages[i]->title, pages[i]->redirect);
			}
		}
	}

	/* delete marked pages */
	for (i = j = 0; i < page_num; i ++) {
		if (pages[i]->title_offset != 0xffffffff)
			pages[j ++] = pages[i];
	}
	page_num = j;

	/* setup finger table */
	for (i = 0; i < 1 << finger_bits; i ++)
		fingers[i] = page_num;
	for (i = page_num - 1; i >= 0; i --)
		fingers[pages[i]->title_hash >> (32 - finger_bits)] = i;
	assert(fwrite(fingers, (1 << finger_bits) * 4, 1, indexfile) == 1);
	printf("finger table size: %d\n", (1 << finger_bits) * 4);
	printf("page entry size: %d\n", (int)sizeof(struct lxp_page_entry) * page_num);

	/* calculate page.title_offset */
	tp_offset = 0;
	for (i = 0; i < page_num; i ++) {
		pages[i]->title_offset = tp_offset;
		tp_offset += strlen(pages[i]->title) + 1;
	}
	printf("text pool 1 size: %d\n", tp_offset);

	/* write page entry */
	for (i = 0; i < page_num; i ++) {
		struct lxp_page_entry page_entry;

		page_entry.title_hash = pages[i]->title_hash;
		page_entry.title_offset = pages[i]->title_offset;
		if (pages[i]->block_num != 0xffffffff) {
			page_entry.block_num = pages[i]->block_num;
			page_entry.block_offset = pages[i]->block_offset;
		} else {
			int pageid = search_redirect(pages[i]->redirect, 5);
			char title[512], *anchor;
			assert(pageid != -1);
			strncpy(title, pages[i]->redirect, sizeof(title));
			title[sizeof(title) - 1] = '\0';
			anchor = strchr(title, '#');
			if (anchor == NULL || anchor[1] == '\0') {
				page_entry.block_num = 0xffffffff;
				page_entry.block_offset = pageid;
			} else {
				page_entry.block_num = 0xfffffffe;
				page_entry.block_offset = tp_offset;
				tp_offset += 4 + strlen(anchor);
			}
		}
		assert(fwrite(&page_entry, sizeof(page_entry), 1, indexfile) == 1);
	}
	printf("text pool 1+2 size: %d\n", tp_offset);

	/* write text pool */
	for (i = 0; i < page_num; i ++) {
		assert(fwrite(pages[i]->title, strlen(pages[i]->title) + 1, 1, indexfile) == 1);
	}
	for (i = 0; i < page_num; i ++) {
		if (pages[i]->block_num == 0xffffffff) {
			int pageid = search_redirect(pages[i]->redirect, 5);
			char title[512], *anchor;
			assert(pageid != -1);
			strncpy(title, pages[i]->redirect, sizeof(title));
			title[sizeof(title) - 1] = '\0';
			anchor = strchr(title, '#');
			if (anchor != NULL && anchor[1] != '\0') {
				assert(fwrite(&pageid, 4, 1, indexfile) == 1);
				assert(fwrite(anchor + 1, strlen(anchor), 1, indexfile) == 1);
			}
		}
	}
	fclose(indexfile);

	if (gen_listing) {
		snprintf(filename, sizeof(filename), "lxpdb-%s.list", language);
		indexfile = fopen(filename, "w");
		if (indexfile == NULL) {
			printf("failed to open %s for writing\n", filename);
			exit(1);
		}
		for (i = 0; i < page_num; i ++) {
			if (pages[i]->block_num == 0xffffffff)
				fprintf(indexfile, "%s\t%s\n", pages[i]->title, pages[i]->redirect);
			else
				fprintf(indexfile, "%s\t%d\t%d\n", pages[i]->title, pages[i]->block_num, pages[i]->block_offset);
		}
		fclose(indexfile);
	}
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
		} else if (strcmp(argv[i], "-l") == 0) {
			gen_listing = 1;
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
