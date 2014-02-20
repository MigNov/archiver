#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <getopt.h>
#include <stdlib.h>

int count;
int len;

char *convert_str = NULL;
char *quartet_str = NULL;

char gQuartet[4] = "CGTA";

void parse_flags(int argc, char * const argv[]) {
	int option_index = 0, c;
	struct option long_options[] = {
		{"count", 1, 0, 'c'},
		{"length", 1, 0, 'l'},
		{"quartet", 1, 0, 'q'},
		{"four-system", 1, 0, 'f'},
		{0, 0, 0, 0}
	};

	while (1) {
		c = getopt_long(argc, argv, "c:l:f:q:",
			long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'c':
				count = atoi(optarg);
				break;
			case 'l':
				len = atoi(optarg);
				break;
			case 'f':
				convert_str = strdup(optarg);
				break;
			case 'q':
				quartet_str = strdup(optarg);
				break;
			default:
				printf("Syntax: %s [--count <count>] [--length <length>] [--four-system <instr> [--quartet <quartet=%s>]]\n", argv[0], gQuartet);
				exit(1);
				break;
		}
	}
}

char *generate_password(int len)
{
	int i;
	char *out = NULL;
	char h[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

	out = (char *)malloc( (len + 1) * sizeof(char) );
	memset(out, 0, len + 1);
	srand( time(NULL) + rand() );
	for (i = 0; i < len; i++)
		out[i] = h[rand() % strlen(h)];

	return out;
}

void four_numbering_system_set_quartet(char *quartet)
{
	strncpy(gQuartet, quartet, 4);
}

int my_power(int num, int exp)
{
	int i = 0;
	int ret = num;

	if (exp == 0)
		return 1;

	for (i = 0; i < exp - 1; i++)
		ret *= num;

	return ret;
}

unsigned char *four_numbering_system_encode(unsigned char *data, int len)
{
	int i, val;
	char a[5] = { 0 };
	unsigned char *output = NULL;

	output = (unsigned char *)malloc( (len * 4) * sizeof(unsigned char) );
	memset(output, 0, (len * 4) * sizeof(unsigned char));
	for (i = 0; i < len; i++) {
		val = data[i];

		memset(a, 0, 5);
		a[0] = gQuartet[(val / 64) % 4];
		a[1] = gQuartet[(val / 16) % 4];
		a[2] = gQuartet[(val / 4 ) % 4];
		a[3] = gQuartet[(val / 1 ) % 4];

		strcat((char *)output, a);
	}

	return output;
}

int find_element_index(const char *str, int c)
{
	int i;

	for (i = 0; i < strlen(str); i++) {
		if (str[i] == c)
			return i;
	}

	return -1;
}

unsigned char *four_numbering_system_decode(unsigned char *data, int len)
{
	int i, j, k, val;
	unsigned char *output = NULL;

	if (len % 4 != 0)
		return NULL;

	output = (unsigned char *)malloc( (len / 4) * sizeof(unsigned char) );
	memset(output, 0, (len / 4) * sizeof(unsigned char) );

	for (i = 0; i < len; i += 4) {
		val = 0;
		for (j = 0; j < strlen(gQuartet); j++) {
			k = find_element_index(gQuartet, data[i+j]);
			if (k < 0) {
				free(output);
				return NULL;
			}
			val += (k * my_power(4, 4 - (j + 1)));
		}

		output[i / 4] = val;
	}

	return output;
}

int four_numbering_system_test(unsigned char *data, int len)
{
	int ret;
	unsigned char *tmp1 = NULL;
	unsigned char *tmp2 = NULL;

	if ((tmp1 = four_numbering_system_encode(data, len)) == NULL)
		return -EIO;

	if ((tmp2 = four_numbering_system_decode(tmp1, len * 4)) == NULL) {
		free(tmp1);
		return -EINVAL;
	}

	ret = (strcmp((char *)data, (char *)tmp2) != 0);
	free(tmp1);
	free(tmp2);

	return ret;
}

int main(int argc, char *argv[])
{
	int i;
	char *pass = NULL;

	len = 8;
	count = 1;

	parse_flags(argc, argv);
	if (four_numbering_system_test((unsigned char *)"test", 4) != 0)
		printf("Warning: Four numbering system test failed!\n");
	else {
		unsigned char *tmp = NULL;
		unsigned char *tmp2 = NULL;

		if (quartet_str != NULL)
			four_numbering_system_set_quartet(quartet_str);

		if (convert_str != NULL) {
			tmp = four_numbering_system_encode((unsigned char *)convert_str, strlen(convert_str));
			tmp2 = four_numbering_system_decode(tmp, strlen((char *)tmp));

			if (strcmp((char *)tmp2, (char *)convert_str) == 0) {
				printf("String '%s' encoded into: '%s'\n", convert_str, tmp);
			}
			else
				printf("Conversion failed\n");

			free(tmp2);
			free(tmp);

			count = 0;
		}
	}

	for (i = 0; i < count; i++) {
		pass = generate_password(len);
		printf("%s\n", pass);
		free(pass);
	}

	return 0;
}

unsigned char *four_numbering_system_encode(unsigned char *data, int len);
unsigned char *four_numbering_system_decode(unsigned char *data, int len);
int four_numbering_system_test(unsigned char *data, int len);
