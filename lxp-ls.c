#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h> /* TODO: assertion must be enabled */
#include <zlib.h>
#include "lxplib.h"

void usage (void)
{
}

void main (int argc, char **argv)
{
	int match_cond = 1; /* 0: exact; 1: contains; 2: shell expansion (*?); 3: posix standard regular expression */
	char *lxp_dir = NULL;
	char *language = NULL;
	char *title_exp = NULL;
	int i, fd;

	for (i = 1; i < argc; i ++) {
		if (strcmp(argv[i], "-h") == 0 ||
				strcmp(argv[i], "--help") == 0 ||
				strcmp(argv[i], "-?") == 0) {
			usage();
			return 0;
		} else if (strcmp(argv[i], "-e") == 0) {
			match_cond = 0;
		} else if (strcmp(argv[i], "-c") == 0) {
			match_cond = 1;
		} else if (strcmp(argv[i], "-s") == 0) {
			match_cond = 2;
		} else if (strcmp(argv[i], "-r") == 0) {
			match_cond = 3;
		} else if (strcmp(argv[i], "-d") == 0) {
			lxp_dir = argv[++ i];
		} else if (argv[i][0] == '-') {
			printf("unknown option %s\n", argv[i]);
			return 1;
		} else {
			if (language != NULL) {
				language = argv[++ i];
			} else if (title_exp != NULL) {
				title_exp = argv[++ i];
			} else {
				fprintf(stderr, "wrong usage\n");
				usage();
			}
			break;
		}
	}

	{
		char index_path[256];
		if (lxp_dir)
			snprintf(index_path, "%s/lxpdb-%s.index", lxp_dir, language);
		else
			snprintf(index_path, "lxpdb-%s.index", language);
		index_path[sizeof(index_path) - 1] = '\0';
		map_index(index_path);
	}
}
