#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <zlib.h>
#include "loki.h"

#define HT_BITS 24 /* 16M */
#define HT_SIZE (1<<HT_BITS)
static struct loki_hentry *hashtable[HT_SIZE];


/* read a line from STDIN.
 * BTW, why is it so difficult ro read a line in C? */
static struct mcs_struct *read_line (void)
{
	static struct mcs_struct *line;

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

static void add_page (struct mcs_struct *title, struct mcs_struct *text)
{
}

int main (int argc, char *argv[])
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
					return 1;
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

	return 0;
}
