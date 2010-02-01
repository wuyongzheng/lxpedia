#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <assert.h>
#include "lxplib.h"

static const char *wikipedia_languages[] = {
	"aa", "ab", "ace", "af", "ak", "als", "am", "an", "ang", "ar", "arc", "arz",
	"as", "ast", "av", "ay", "az", "ba", "bar", "bat-smg", "bcl", "be", "be-x-old",
	"bg", "bh", "bi", "bm", "bn", "bo", "bpy", "br", "bs", "bug", "bxr", "ca",
	"cbk-zam", "cdo", "ce", "ceb", "ch", "cho", "chr", "chy", "ckb", "co", "cr",
	"crh", "cs", "csb", "cu", "cv", "cy", "da", "de", "diq", "dsb", "dv", "dz",
	"ee", "el", "eml", "en", "eo", "es", "et", "eu", "ext", "fa", "ff", "fi",
	"fiu-vro", "fj", "fo", "fr", "frp", "fur", "fy", "ga", "gan", "gd", "gl",
	"glk", "gn", "got", "gu", "gv", "ha", "hak", "haw", "he", "hi", "hif", "ho",
	"hr", "hsb", "ht", "hu", "hy", "hz", "ia", "id", "ie", "ig", "ii", "ik", "ilo",
	"io", "is", "it", "iu", "ja", "jbo", "jv", "ka", "kaa", "kab", "kg", "ki",
	"kj", "kk", "kl", "km", "kn", "ko", "kr", "ks", "ksh", "ku", "kv", "kw", "ky",
	"la", "lad", "lb", "lbe", "lg", "li", "lij", "lmo", "ln", "lo", "lt", "lv",
	"map-bms", "mdf", "mg", "mh", "mhr", "mi", "mk", "ml", "mn", "mo", "mr", "ms",
	"mt", "mus", "mwl", "my", "myv", "mzn", "na", "nah", "nap", "nds", "nds-nl",
	"ne", "new", "ng", "nl", "nn", "no", "nov", "nrm", "nv", "ny", "oc", "om",
	"or", "os", "pa", "pag", "pam", "pap", "pcd", "pdc", "pi", "pih", "pl", "pms",
	"pnb", "pnt", "ps", "pt", "qu", "rm", "rmy", "rn", "ro", "roa-rup", "roa-tara",
	"ru", "rw", "sa", "sah", "sc", "scn", "sco", "sd", "se", "sg", "sh", "si",
	"simple", "sk", "sl", "sm", "sn", "so", "sq", "sr", "srn", "ss", "st", "stq",
	"su", "sv", "sw", "szl", "ta", "te", "tet", "tg", "th", "ti", "tk", "tl", "tn",
	"to", "tokipona", "tpi", "tr", "ts", "tt", "tum", "tw", "ty", "udm", "ug",
	"uk", "ur", "uz", "ve", "vec", "vi", "vls", "vo", "wa", "war", "wo", "wuu",
	"xal", "xh", "yi", "yo", "za", "zea", "zh", "zh-classical", "zh-min-nan",
	"zh-yue", "zu", NULL};
static const char *wikipedia_namespaces[] = {
	"User", "Wikipedia", "File", "MediaWiki", "Template", "Help", "Category",
	"Portal", "Book", "Talk", "User talk", "Wikipedia talk", "File talk",
	"MediaWiki talk", "Template talk", "Help talk", "Category talk",
	"Portal talk", "Book talk", "Special", "Media", NULL};
static const char *wikipedia_namespace_aliases[] = {
	"WP", "Wikipedia",
	"Project", "Wikipedia",
	"WT", "Wikipedia talk",
	"Project talk", "Wikipedia talk",
	"Image", "File",
	"Image talk", "File talk",
	NULL, NULL};

/* Refered from Title.php newFromText()
 * return 1 if success */
int parse_title_from_text (const char *_text, int text_len,
		char *title, int title_size,
		char *anchor, int anchor_size,
		char *interwiki, int interwiki_size,
		char *namespace, int namespace_size,
		char *label, int label_size)
{
	char text_buff[512], *text = text_buff, *ptr;
	int i;

	if (title_size) title[0] = '\0';
	if (anchor_size) anchor[0] = '\0';
	if (interwiki_size) interwiki[0] = '\0';
	if (namespace_size) namespace[0] = '\0';
	if (label_size) label[0] = '\0';

	if (text_len < 0)
		text_len = strlen(_text);
	if (text_len > sizeof(text_buff) - 10)
		text_len = sizeof(text_buff) - 10;
	memcpy(text, _text, text_len);
	text[text_len] = '\0';
	if (text[0] == '\0')
		return 0;

	/* convert tab, newline and underscore to space */
	for (ptr = text; *ptr; ptr ++)
		if (*(unsigned char *)ptr < ' ' || *ptr == '_')
			*ptr = ' ';

	while (decode_url_encoding(text) || decode_html_entity_full(text))
		;

#define REPLACE_ALL(_search_,_replace_) { \
		char *_ptr_ = strstr(text, _search_); \
		while (_ptr_) { \
			memmove(_ptr_ + strlen(_replace_), \
					_ptr_ + strlen(_search_), \
					strlen(_ptr_ + strlen(_search_)) + 1); \
			memcpy(_ptr_, _replace_, strlen(_replace_)); \
			_ptr_ = strstr(text, _search_); \
		} \
	}
	REPLACE_ALL("\xE2\x80\x8E", "");
	REPLACE_ALL("\xE2\x80\x8F", "");
	REPLACE_ALL("\xE2\x80\xAA", "");
	REPLACE_ALL("\xE2\x80\xAB", "");
	REPLACE_ALL("\xE2\x80\xAC", "");
	REPLACE_ALL("\xE2\x80\xAD", "");
	REPLACE_ALL("\xE2\x80\xAE", "");
	REPLACE_ALL("_", " ");
	REPLACE_ALL("  ", " ");
#undef REPLACE_ALL
	while (*text == ' ')
		text ++;
	if (text[0] != '\0' && text[strlen(text) - 1] == ' ')
		text[strlen(text) - 1] = '\0';
	if (text[0] == '\0')
		return 0;

	for (i = 0; wikipedia_languages[i]; i ++) {
		const char *lan = wikipedia_languages[i];
		int lan_len = strlen(lan);
		if (strncasecmp(text, lan, lan_len) != 0)
			continue;
		if (text[lan_len] == ':')
			text += text[lan_len + 1] == ' ' ? lan_len + 2 : lan_len + 1;
		else if (text[lan_len] == ' ' && text[lan_len + 1] == ':')
			text += text[lan_len + 2] == ' ' ? lan_len + 3 : lan_len + 2;
		else
			continue;
		if (interwiki_size)
			mystrncpy(interwiki, lan, interwiki_size);
		break;
	}
	for (i = 0; wikipedia_namespace_aliases[i]; i += 2) {
		const char *alias = wikipedia_namespace_aliases[i];
		const char *realns = wikipedia_namespace_aliases[i+1];
		int alias_len = strlen(alias);
		if (strncasecmp(text, alias, alias_len) != 0)
			continue;
		if (text[alias_len] == ':')
			text += text[alias_len + 1] == ' ' ? alias_len + 2 : alias_len + 1;
		else if (text[alias_len] == ' ' && text[alias_len + 1] == ':')
			text += text[alias_len + 2] == ' ' ? alias_len + 3 : alias_len + 2;
		else
			continue;
		if (namespace_size)
			mystrncpy(namespace, realns, namespace_size);
		goto nsdone;
	}
	for (i = 0; wikipedia_namespaces[i]; i ++) {
		const char *ns = wikipedia_namespaces[i];
		int ns_len = strlen(ns);
		if (strncasecmp(text, ns, ns_len) != 0)
			continue;
		if (text[ns_len] == ':')
			text += text[ns_len + 1] == ' ' ? ns_len + 2 : ns_len + 1;
		else if (text[ns_len] == ' ' && text[ns_len + 1] == ':')
			text += text[ns_len + 2] == ' ' ? ns_len + 3 : ns_len + 2;
		else
			continue;
		if (namespace_size)
			mystrncpy(namespace, ns, namespace_size);
		break;
	}
nsdone:

	if ((ptr = strchr(text, '|')) != NULL) {
		if (ptr != text && *(ptr - 1) == ' ')
			*(ptr - 1) = '\0';
		else
			*ptr = '\0';
		ptr += *(ptr + 1) == ' ' ? 2 : 1;
		if (label_size) {
			if (*ptr == '\0')
				mystrncpy(label, text, label_size);
			else
				mystrncpy(label, ptr, label_size);
		}
	}

	if ((ptr = strchr(text, '#')) != NULL) {
		if (ptr != text && *(ptr - 1) == ' ')
			*(ptr - 1) = '\0';
		else
			*ptr = '\0';
		ptr += *(ptr + 1) == ' ' ? 2 : 1;
		if (anchor_size)
			mystrncpy(anchor, ptr, anchor_size);
	}

	first_toupper(text);

	if (title_size)
		mystrncpy(title, text, title_size);

	return 1;
}

int parse_title_from_text_keepns (const char *text, int text_len,
		char *title, int title_size,
		char *anchor, int anchor_size,
		char *interwiki, int interwiki_size,
		char *label, int label_size)
{
	int retval;
	char namespace[32];

	retval = parse_title_from_text(text, text_len, title, title_size,
			anchor, anchor_size, interwiki, interwiki_size,
			namespace, sizeof(namespace), label, label_size);
	if (retval && title_size && namespace[0]) {
		int ns_len = strlen(namespace);
		if (title_size <= ns_len + 1) {
			mystrncpy(title, namespace, title_size);
		} else {
			int title_len = strlen(title);
			if (title_len > title_size - ns_len - 2)
				title_len = title_size - ns_len - 2;
			memmove(title + ns_len + 1, title, title_len + 1);
			memcpy(title, namespace, ns_len);
			title[ns_len] = ':';
			title[ns_len + 1 + title_len] = '\0';
		}
	}
	return retval;
}

/* Try to parse title from #REDIRECT[[XXX]].
 * Referred to Title.php: newFromRedirectInternal()
 * Return 1 if success */
int parse_title_from_redirect_keepns (const char *_text, int text_len,
		char *title, int title_size,
		char *anchor, int anchor_size,
		char *interwiki, int interwiki_size,
		char *label, int label_size)
{
	static regex_t reg;
	static int reg_init = 0;
	char text[512], *ptr;
	regmatch_t matches[2];
	int retval;

	if (title_size) title[0] = '\0';
	if (anchor_size) anchor[0] = '\0';
	if (interwiki_size) interwiki[0] = '\0';
	if (label_size) label[0] = '\0';

	if (text_len < 0)
		text_len = strlen(_text);
	if (text_len > sizeof(text) - 10)
		text_len = sizeof(text) - 10;
	memcpy(text, _text, text_len);
	text[text_len] = '\0';

	/* convert tab, newline and underscore to space */
	for (ptr = text; *ptr; ptr ++) {
		if (*(unsigned char *)ptr < ' ' || *ptr == '_')
			*ptr = ' ';
		else if (*ptr == '[') // TODO i'm stuck with the regxp problem: why [\[\]]* doesn't work?
			*ptr = '<';
		else if (*ptr == ']')
			*ptr = '>';
	}

	if (!reg_init) {
		assert(regcomp(&reg, "^ *#redirect *:? *<<[ :]*([^<>]+)>>", REG_ICASE | REG_EXTENDED) == 0);
		reg_init = 1;
	}
	retval = regexec(&reg, text, 2, matches, 0);
	if (retval == REG_NOMATCH)
		return 0;
	assert(retval == 0);
	assert(matches[1].rm_so >= 0);
	assert(matches[1].rm_eo > matches[1].rm_so);
	ptr = text + matches[1].rm_so;
	text[matches[1].rm_eo] = '\0';

	return parse_title_from_text_keepns(ptr, matches[1].rm_eo - matches[1].rm_so,
			title, title_size, anchor, anchor_size,
			interwiki, interwiki_size, label, label_size);
}
