#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lxplib.h"

static const char *samples_url[] = {
	"",
	"",
	"a",
	"a",
	"vfdsvfsvm%1gf sgdf%fg;gf gfdv fds",
	"vfdsvfsvm%1gf sgdf%fg;gf gfdv fds",
	"%20",
	" ",
	"%20x",
	" x",
	"x%20",
	"x ",
	"%20%20%20",
	"   ",
	NULL, NULL
};
static const char *samples_ent[] = {
	"",
	"",
	"a",
	"a",
	"vfdsvfsvm&gf sgdf&;gf gfdv fds",
	"vfdsvfsvm&gf sgdf&;gf gfdv fds",
	"&amp;",
	"&",
	"&amp;x",
	"&x",
	"x&amp;",
	"x&",
	"x&amp;amp;",
	"x&amp;",
	"&lt;a href=&quot;http://www.example.com&quot;&gt;example&lt;/a&gt;",
	"<a href=\"http://www.example.com\">example</a>",
	NULL, NULL
};
static const char *samples_more_ent[] = {
	"&#60;a href=&#34;http://www.example.com&#34;&#62;example&#60;/a&#62;",
	"<a href=\"http://www.example.com\">example</a>",
	"&#x3C;a href=&#x22;http://www.example.com&#x22;&#x3e;example&#x3C;/a&#x3E;",
	"<a href=\"http://www.example.com\">example</a>",
	NULL, NULL
};

int main (void)
{
	int i;
	char buff[1024];

	for (i = 0; samples_url[i]; i += 2) {
		strcpy(buff, samples_url[i]);
		decode_url_encoding(buff);
		if (strcmp(buff, samples_url[i+1]) != 0) {
			printf("decode_url_encoding: in=\"%s\" correct=\"%s\" out=\"%s\"\n",
					samples_url[i], samples_url[i+1], buff);
			return 1;
		}
	}

	for (i = 0; samples_ent[i]; i += 2) {
		strcpy(buff, samples_ent[i]);
		decode_html_entity_minimal(buff);
		if (strcmp(buff, samples_ent[i+1]) != 0) {
			printf("decode_html_entity_minimal: in=\"%s\" correct=\"%s\" out=\"%s\"\n",
					samples_ent[i], samples_ent[i+1], buff);
			return 1;
		}
	}

	for (i = 0; samples_ent[i]; i += 2) {
		strcpy(buff, samples_ent[i]);
		decode_html_entity_full(buff);
		if (strcmp(buff, samples_ent[i+1]) != 0) {
			printf("decode_html_entity_full: in=\"%s\" correct=\"%s\" out=\"%s\"\n",
					samples_ent[i], samples_ent[i+1], buff);
			return 1;
		}
	}
	for (i = 0; samples_more_ent[i]; i += 2) {
		strcpy(buff, samples_more_ent[i]);
		decode_html_entity_full(buff);
		if (strcmp(buff, samples_more_ent[i+1]) != 0) {
			printf("decode_html_entity_full: in=\"%s\" correct=\"%s\" out=\"%s\"\n",
					samples_more_ent[i], samples_more_ent[i+1], buff);
			return 1;
		}
	}

	return 0;
}
