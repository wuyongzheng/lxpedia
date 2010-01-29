#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lxplib.h"

#define ISHEX(c) (((c) >= '0' && (c) <= '9') || \
	((c) >= 'A' && (c) <= 'f') || \
	((c) >= 'a' && (c) <= 'f'))
#define HEXVAL(c) ((unsigned char)(c) >= 'a' ? \
	(unsigned char)(c)-'a'+10 : \
	(unsigned char)(c) >= 'A' ? \
		(unsigned char)(c)-'A'+10 : \
		(unsigned char)(c)-'0')

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

/* given a unicode character "unival", generate the UTF-8
 * string in "outstr", return the length of it.
 * The output string is NOT null terminated. */
int encode_utf8 (char *outstr, unsigned int unival)
{
	if (unival < 0x80) {
		outstr[0] = unival;
		return 1;
	}
	if (unival < 0x800) {
		outstr[0] = 0xc0 | (unival >> 6);
		outstr[1] = 0x80 | (unival & 0x3f);
		return 2;
	}
	if (unival < 0x10000) {
		outstr[0] = 0xe0 | (unival >> 12);
		outstr[1] = 0x80 | ((unival >> 6) & 0x3f);
		outstr[2] = 0x80 | (unival & 0x3f);
		return 3;
	}
	if (unival < 0x110000) {
		outstr[0] = 0xf0 | (unival >> 18);
		outstr[1] = 0x80 | ((unival >> 12) & 0x3f);
		outstr[2] = 0x80 | ((unival >> 6) & 0x3f);
		outstr[3] = 0x80 | (unival & 0x3f);
		return 4;
	}
	assert(0);
}

/* decode things like %20 */
int decode_url_encoding (char *str)
{
	char *inptr, *outptr;
	int count;

	inptr = strchr(str, '%');
	if (!inptr)
		return 0;

	outptr = inptr;
	count = 0;
	while (*inptr) {
		if (*inptr == '%' && ISHEX(inptr[1]) && ISHEX(inptr[2])) {
			*(outptr++) = HEXVAL(inptr[1]) * 16 + HEXVAL(inptr[2]);
			inptr += 3;
			count ++;
			continue;
		}
		*(outptr++) = *(inptr++);
	}
	*outptr = '\0';
	return count;
}

/* only decode the must-escape ones: &quot; &amp; &apos; &lt; &gt;
 * it's used to extract text from wiki xml dump */
int decode_html_entity_minimal (char *str)
{
	char *inptr, *outptr;
	int count;

	inptr = strchr(str, '&');
	if (!inptr)
		return 0;

	outptr = inptr;
	count = 0;
	while (*inptr) {
		if (*inptr != '&')
			goto nomatch;
		if (memcmp(inptr, "&quot;", 6) == 0) {*(outptr++) = '"';  inptr += 6; count++; continue;}
		if (memcmp(inptr, "&amp;",  5) == 0) {*(outptr++) = '&';  inptr += 5; count++; continue;}
		if (memcmp(inptr, "&apos;", 6) == 0) {*(outptr++) = '\''; inptr += 6; count++; continue;}
		if (memcmp(inptr, "&lt;",   4) == 0) {*(outptr++) = '<';  inptr += 4; count++; continue;}
		if (memcmp(inptr, "&gt;",   4) == 0) {*(outptr++) = '>';  inptr += 4; count++; continue;}
nomatch:
		*(outptr++) = *(inptr++);
	}
	*outptr = '\0';
	return count;
}

struct entity_struct {
	int name_len;
	int value_len;
	const char *name;
	const char *value;
};
/* They ought to be sorted by probability, higher probability goes in front. */
const struct entity_struct entities_full[] = {
	{6, 1, "&quot;", "\x22"},
	{5, 1, "&amp;", "\x26"},
	{6, 1, "&apos;", "\x27"},
	{4, 1, "&lt;", "\x3c"},
	{4, 1, "&gt;", "\x3e"},
	{6, 2, "&nbsp;", "\xc2\xa0"},
	{7, 2, "&iexcl;", "\xc2\xa1"},
	{6, 2, "&cent;", "\xc2\xa2"},
	{7, 2, "&pound;", "\xc2\xa3"},
	{8, 2, "&curren;", "\xc2\xa4"},
	{5, 2, "&yen;", "\xc2\xa5"},
	{8, 2, "&brvbar;", "\xc2\xa6"},
	{6, 2, "&sect;", "\xc2\xa7"},
	{5, 2, "&uml;", "\xc2\xa8"},
	{6, 2, "&copy;", "\xc2\xa9"},
	{6, 2, "&ordf;", "\xc2\xaa"},
	{7, 2, "&laquo;", "\xc2\xab"},
	{5, 2, "&not;", "\xc2\xac"},
	{5, 2, "&shy;", "\xc2\xad"},
	{5, 2, "&reg;", "\xc2\xae"},
	{6, 2, "&macr;", "\xc2\xaf"},
	{5, 2, "&deg;", "\xc2\xb0"},
	{8, 2, "&plusmn;", "\xc2\xb1"},
	{6, 2, "&sup2;", "\xc2\xb2"},
	{6, 2, "&sup3;", "\xc2\xb3"},
	{7, 2, "&acute;", "\xc2\xb4"},
	{7, 2, "&micro;", "\xc2\xb5"},
	{6, 2, "&para;", "\xc2\xb6"},
	{8, 2, "&middot;", "\xc2\xb7"},
	{7, 2, "&cedil;", "\xc2\xb8"},
	{6, 2, "&sup1;", "\xc2\xb9"},
	{6, 2, "&ordm;", "\xc2\xba"},
	{7, 2, "&raquo;", "\xc2\xbb"},
	{8, 2, "&frac14;", "\xc2\xbc"},
	{8, 2, "&frac12;", "\xc2\xbd"},
	{8, 2, "&frac34;", "\xc2\xbe"},
	{8, 2, "&iquest;", "\xc2\xbf"},
	{8, 2, "&Agrave;", "\xc3\x80"},
	{8, 2, "&Aacute;", "\xc3\x81"},
	{7, 2, "&Acirc;", "\xc3\x82"},
	{8, 2, "&Atilde;", "\xc3\x83"},
	{6, 2, "&Auml;", "\xc3\x84"},
	{7, 2, "&Aring;", "\xc3\x85"},
	{7, 2, "&AElig;", "\xc3\x86"},
	{8, 2, "&Ccedil;", "\xc3\x87"},
	{8, 2, "&Egrave;", "\xc3\x88"},
	{8, 2, "&Eacute;", "\xc3\x89"},
	{7, 2, "&Ecirc;", "\xc3\x8a"},
	{6, 2, "&Euml;", "\xc3\x8b"},
	{8, 2, "&Igrave;", "\xc3\x8c"},
	{8, 2, "&Iacute;", "\xc3\x8d"},
	{7, 2, "&Icirc;", "\xc3\x8e"},
	{6, 2, "&Iuml;", "\xc3\x8f"},
	{5, 2, "&ETH;", "\xc3\x90"},
	{8, 2, "&Ntilde;", "\xc3\x91"},
	{8, 2, "&Ograve;", "\xc3\x92"},
	{8, 2, "&Oacute;", "\xc3\x93"},
	{7, 2, "&Ocirc;", "\xc3\x94"},
	{8, 2, "&Otilde;", "\xc3\x95"},
	{6, 2, "&Ouml;", "\xc3\x96"},
	{7, 2, "&times;", "\xc3\x97"},
	{8, 2, "&Oslash;", "\xc3\x98"},
	{8, 2, "&Ugrave;", "\xc3\x99"},
	{8, 2, "&Uacute;", "\xc3\x9a"},
	{7, 2, "&Ucirc;", "\xc3\x9b"},
	{6, 2, "&Uuml;", "\xc3\x9c"},
	{8, 2, "&Yacute;", "\xc3\x9d"},
	{7, 2, "&THORN;", "\xc3\x9e"},
	{7, 2, "&szlig;", "\xc3\x9f"},
	{8, 2, "&agrave;", "\xc3\xa0"},
	{8, 2, "&aacute;", "\xc3\xa1"},
	{7, 2, "&acirc;", "\xc3\xa2"},
	{8, 2, "&atilde;", "\xc3\xa3"},
	{6, 2, "&auml;", "\xc3\xa4"},
	{7, 2, "&aring;", "\xc3\xa5"},
	{7, 2, "&aelig;", "\xc3\xa6"},
	{8, 2, "&ccedil;", "\xc3\xa7"},
	{8, 2, "&egrave;", "\xc3\xa8"},
	{8, 2, "&eacute;", "\xc3\xa9"},
	{7, 2, "&ecirc;", "\xc3\xaa"},
	{6, 2, "&euml;", "\xc3\xab"},
	{8, 2, "&igrave;", "\xc3\xac"},
	{8, 2, "&iacute;", "\xc3\xad"},
	{7, 2, "&icirc;", "\xc3\xae"},
	{6, 2, "&iuml;", "\xc3\xaf"},
	{5, 2, "&eth;", "\xc3\xb0"},
	{8, 2, "&ntilde;", "\xc3\xb1"},
	{8, 2, "&ograve;", "\xc3\xb2"},
	{8, 2, "&oacute;", "\xc3\xb3"},
	{7, 2, "&ocirc;", "\xc3\xb4"},
	{8, 2, "&otilde;", "\xc3\xb5"},
	{6, 2, "&ouml;", "\xc3\xb6"},
	{8, 2, "&divide;", "\xc3\xb7"},
	{8, 2, "&oslash;", "\xc3\xb8"},
	{8, 2, "&ugrave;", "\xc3\xb9"},
	{8, 2, "&uacute;", "\xc3\xba"},
	{7, 2, "&ucirc;", "\xc3\xbb"},
	{6, 2, "&uuml;", "\xc3\xbc"},
	{8, 2, "&yacute;", "\xc3\xbd"},
	{7, 2, "&thorn;", "\xc3\xbe"},
	{6, 2, "&yuml;", "\xc3\xbf"},
	{6, 2, "&fnof;", "\xc6\x92"},
	{7, 2, "&Alpha;", "\xce\x91"},
	{6, 2, "&Beta;", "\xce\x92"},
	{7, 2, "&Gamma;", "\xce\x93"},
	{7, 2, "&Delta;", "\xce\x94"},
	{9, 2, "&Epsilon;", "\xce\x95"},
	{6, 2, "&Zeta;", "\xce\x96"},
	{5, 2, "&Eta;", "\xce\x97"},
	{7, 2, "&Theta;", "\xce\x98"},
	{6, 2, "&Iota;", "\xce\x99"},
	{7, 2, "&Kappa;", "\xce\x9a"},
	{8, 2, "&Lambda;", "\xce\x9b"},
	{4, 2, "&Mu;", "\xce\x9c"},
	{4, 2, "&Nu;", "\xce\x9d"},
	{4, 2, "&Xi;", "\xce\x9e"},
	{9, 2, "&Omicron;", "\xce\x9f"},
	{4, 2, "&Pi;", "\xce\xa0"},
	{5, 2, "&Rho;", "\xce\xa1"},
	{7, 2, "&Sigma;", "\xce\xa3"},
	{5, 2, "&Tau;", "\xce\xa4"},
	{9, 2, "&Upsilon;", "\xce\xa5"},
	{5, 2, "&Phi;", "\xce\xa6"},
	{5, 2, "&Chi;", "\xce\xa7"},
	{5, 2, "&Psi;", "\xce\xa8"},
	{7, 2, "&Omega;", "\xce\xa9"},
	{7, 2, "&alpha;", "\xce\xb1"},
	{6, 2, "&beta;", "\xce\xb2"},
	{7, 2, "&gamma;", "\xce\xb3"},
	{7, 2, "&delta;", "\xce\xb4"},
	{9, 2, "&epsilon;", "\xce\xb5"},
	{6, 2, "&zeta;", "\xce\xb6"},
	{5, 2, "&eta;", "\xce\xb7"},
	{7, 2, "&theta;", "\xce\xb8"},
	{6, 2, "&iota;", "\xce\xb9"},
	{7, 2, "&kappa;", "\xce\xba"},
	{8, 2, "&lambda;", "\xce\xbb"},
	{4, 2, "&mu;", "\xce\xbc"},
	{4, 2, "&nu;", "\xce\xbd"},
	{4, 2, "&xi;", "\xce\xbe"},
	{9, 2, "&omicron;", "\xce\xbf"},
	{4, 2, "&pi;", "\xcf\x80"},
	{5, 2, "&rho;", "\xcf\x81"},
	{8, 2, "&sigmaf;", "\xcf\x82"},
	{7, 2, "&sigma;", "\xcf\x83"},
	{5, 2, "&tau;", "\xcf\x84"},
	{9, 2, "&upsilon;", "\xcf\x85"},
	{5, 2, "&phi;", "\xcf\x86"},
	{5, 2, "&chi;", "\xcf\x87"},
	{5, 2, "&psi;", "\xcf\x88"},
	{7, 2, "&omega;", "\xcf\x89"},
	{10, 2, "&thetasym;", "\xcf\x91"},
	{7, 2, "&upsih;", "\xcf\x92"},
	{5, 2, "&piv;", "\xcf\x96"},
	{6, 3, "&bull;", "\xe2\x80\xa2"},
	{8, 3, "&hellip;", "\xe2\x80\xa6"},
	{7, 3, "&prime;", "\xe2\x80\xb2"},
	{7, 3, "&Prime;", "\xe2\x80\xb3"},
	{7, 3, "&oline;", "\xe2\x80\xbe"},
	{7, 3, "&frasl;", "\xe2\x81\x84"},
	{8, 3, "&weierp;", "\xe2\x84\x98"},
	{7, 3, "&image;", "\xe2\x84\x91"},
	{6, 3, "&real;", "\xe2\x84\x9c"},
	{7, 3, "&trade;", "\xe2\x84\xa2"},
	{9, 3, "&alefsym;", "\xe2\x84\xb5"},
	{6, 3, "&larr;", "\xe2\x86\x90"},
	{6, 3, "&uarr;", "\xe2\x86\x91"},
	{6, 3, "&rarr;", "\xe2\x86\x92"},
	{6, 3, "&darr;", "\xe2\x86\x93"},
	{6, 3, "&harr;", "\xe2\x86\x94"},
	{7, 3, "&crarr;", "\xe2\x86\xb5"},
	{6, 3, "&lArr;", "\xe2\x87\x90"},
	{6, 3, "&uArr;", "\xe2\x87\x91"},
	{6, 3, "&rArr;", "\xe2\x87\x92"},
	{6, 3, "&dArr;", "\xe2\x87\x93"},
	{6, 3, "&hArr;", "\xe2\x87\x94"},
	{8, 3, "&forall;", "\xe2\x88\x80"},
	{6, 3, "&part;", "\xe2\x88\x82"},
	{7, 3, "&exist;", "\xe2\x88\x83"},
	{7, 3, "&empty;", "\xe2\x88\x85"},
	{7, 3, "&nabla;", "\xe2\x88\x87"},
	{6, 3, "&isin;", "\xe2\x88\x88"},
	{7, 3, "&notin;", "\xe2\x88\x89"},
	{4, 3, "&ni;", "\xe2\x88\x8b"},
	{6, 3, "&prod;", "\xe2\x88\x8f"},
	{5, 3, "&sum;", "\xe2\x88\x91"},
	{7, 3, "&minus;", "\xe2\x88\x92"},
	{8, 3, "&lowast;", "\xe2\x88\x97"},
	{7, 3, "&radic;", "\xe2\x88\x9a"},
	{6, 3, "&prop;", "\xe2\x88\x9d"},
	{7, 3, "&infin;", "\xe2\x88\x9e"},
	{5, 3, "&ang;", "\xe2\x88\xa0"},
	{5, 3, "&and;", "\xe2\x88\xa7"},
	{4, 3, "&or;", "\xe2\x88\xa8"},
	{5, 3, "&cap;", "\xe2\x88\xa9"},
	{5, 3, "&cup;", "\xe2\x88\xaa"},
	{5, 3, "&int;", "\xe2\x88\xab"},
	{8, 3, "&there4;", "\xe2\x88\xb4"},
	{5, 3, "&sim;", "\xe2\x88\xbc"},
	{6, 3, "&cong;", "\xe2\x89\x85"},
	{7, 3, "&asymp;", "\xe2\x89\x88"},
	{4, 3, "&ne;", "\xe2\x89\xa0"},
	{7, 3, "&equiv;", "\xe2\x89\xa1"},
	{4, 3, "&le;", "\xe2\x89\xa4"},
	{4, 3, "&ge;", "\xe2\x89\xa5"},
	{5, 3, "&sub;", "\xe2\x8a\x82"},
	{5, 3, "&sup;", "\xe2\x8a\x83"},
	{6, 3, "&nsub;", "\xe2\x8a\x84"},
	{6, 3, "&sube;", "\xe2\x8a\x86"},
	{6, 3, "&supe;", "\xe2\x8a\x87"},
	{7, 3, "&oplus;", "\xe2\x8a\x95"},
	{8, 3, "&otimes;", "\xe2\x8a\x97"},
	{6, 3, "&perp;", "\xe2\x8a\xa5"},
	{6, 3, "&sdot;", "\xe2\x8b\x85"},
	{7, 3, "&lceil;", "\xe2\x8c\x88"},
	{7, 3, "&rceil;", "\xe2\x8c\x89"},
	{8, 3, "&lfloor;", "\xe2\x8c\x8a"},
	{8, 3, "&rfloor;", "\xe2\x8c\x8b"},
	{6, 3, "&lang;", "\xe2\x8c\xa9"},
	{6, 3, "&rang;", "\xe2\x8c\xaa"},
	{5, 3, "&loz;", "\xe2\x97\x8a"},
	{8, 3, "&spades;", "\xe2\x99\xa0"},
	{7, 3, "&clubs;", "\xe2\x99\xa3"},
	{8, 3, "&hearts;", "\xe2\x99\xa5"},
	{7, 3, "&diams;", "\xe2\x99\xa6"},
	{7, 2, "&OElig;", "\xc5\x92"},
	{7, 2, "&oelig;", "\xc5\x93"},
	{8, 2, "&Scaron;", "\xc5\xa0"},
	{8, 2, "&scaron;", "\xc5\xa1"},
	{6, 2, "&Yuml;", "\xc5\xb8"},
	{6, 2, "&circ;", "\xcb\x86"},
	{7, 2, "&tilde;", "\xcb\x9c"},
	{6, 3, "&ensp;", "\xe2\x80\x82"},
	{6, 3, "&emsp;", "\xe2\x80\x83"},
	{8, 3, "&thinsp;", "\xe2\x80\x89"},
	{6, 3, "&zwnj;", "\xe2\x80\x8c"},
	{5, 3, "&zwj;", "\xe2\x80\x8d"},
	{5, 3, "&lrm;", "\xe2\x80\x8e"},
	{5, 3, "&rlm;", "\xe2\x80\x8f"},
	{7, 3, "&ndash;", "\xe2\x80\x93"},
	{7, 3, "&mdash;", "\xe2\x80\x94"},
	{7, 3, "&lsquo;", "\xe2\x80\x98"},
	{7, 3, "&rsquo;", "\xe2\x80\x99"},
	{7, 3, "&sbquo;", "\xe2\x80\x9a"},
	{7, 3, "&ldquo;", "\xe2\x80\x9c"},
	{7, 3, "&rdquo;", "\xe2\x80\x9d"},
	{7, 3, "&bdquo;", "\xe2\x80\x9e"},
	{8, 3, "&dagger;", "\xe2\x80\xa0"},
	{8, 3, "&Dagger;", "\xe2\x80\xa1"},
	{8, 3, "&permil;", "\xe2\x80\xb0"},
	{8, 3, "&lsaquo;", "\xe2\x80\xb9"},
	{8, 3, "&rsaquo;", "\xe2\x80\xba"},
	{6, 3, "&euro;", "\xe2\x82\xac"},
	{0, 0, NULL, NULL}
};

/* decode things like &eacute; &#257; or &#x3017; */
int decode_html_entity_full (char *str)
{
	char *inptr, *outptr;
	int count;

	inptr = strchr(str, '&');
	if (!inptr)
		return 0;

	outptr = inptr;
	count = 0;
	while(*inptr) {
		const struct entity_struct *entity;
		const char *replace;
		char replace_buff[5];
		int match_len, replace_len;

		if (*inptr != '&')
			goto nomatch;
		for (entity = entities_full; entity->name_len; entity ++) {
			if (memcmp(inptr, entity->name, entity->name_len) == 0) {
				replace = entity->value;
				match_len = entity->name_len;
				replace_len = entity->value_len;
				goto match;
			}
		}
		if (*(inptr+1) != '#')
			goto nomatch;
		if (*(inptr+2) == 'x') { /* &#x3017; */
			int digits, unival;
			for (digits = 0, unival = 0; digits < 7 && ISHEX(*(inptr+3+digits)); digits ++)
				unival = unival * 16 + HEXVAL(*(inptr+3+digits));
			if (digits == 0 || *(inptr+3+digits) != ';')
				goto nomatch;
			replace_len = encode_utf8(replace_buff, unival);
			replace = replace_buff;
			match_len = digits + 4;
		} else { /* &#257; */
			int digits, unival;
			for (digits = 0, unival = 0; digits < 8 && *(inptr+2+digits) >= '0' &&
					*(inptr+2+digits) <= '9'; digits ++)
				unival = unival * 10 + *(inptr+2+digits) - '0';
			if (digits == 0 || *(inptr+2+digits) != ';')
				goto nomatch;
			replace_len = encode_utf8(replace_buff, unival);
			replace = replace_buff;
			match_len = digits + 3;
		}

match:
		memcpy(outptr, replace, replace_len);
		inptr += match_len;
		outptr += replace_len;
		count ++;
		continue;

nomatch:
		*(outptr++) = *(inptr++);
	}
	*outptr = '\0';
	return count;
}
