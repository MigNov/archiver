#ifndef ARCHIVER_H
#define ARCHIVER_H

/* Don't use LARGEFILE by default, causes issues */
//#define USE_LARGE_FILE
#define DEBUG_ARCHIVER_ALL

#ifdef DEBUG_ARCHIVER_ALL
	#define DEBUG_ARCHIVE
	#define DEBUG_LZMA
	#define DEBUG_CRC
	#define DEBUG_ARCHIVER
#endif

#define BUFFER_SIZE			(1 << 16)      /* Make 64 kB to be default buffer size */

#ifdef USE_LARGE_FILE
#define O_LARGEFILE                     0x0200000
#endif

#if O_LARGEFILE
#define O_LFILE				O_LARGEFILE
#else
#define O_LFILE				0
#endif

#define COMPRESSION_LEVEL		9
#define SIGNATURE			"CAF"
#define ARCHIVER_VERSION		1
#define HEADER_SIZE_OFFSET		10
#define DEFAULT_SALT_VAL		SIGNATURE
#define VECTOR_MULTIPLIER		1 << 7

#define	FLAG_ARCHIVE_CREATE		0x1
#define	FLAG_ARCHIVE_APPEND		0x2
#define	FLAG_ARCHIVE_LIST		0x4
#define	FLAG_ARCHIVE_EXTRACT		0x8
#define	FLAG_ARCHIVE_SALT		0x10
#define	FLAG_ARCHIVE_PASSWORD		0x20
#define	FLAG_OUTPUT_HEX			0x40

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <lzma.h>
#include <sys/stat.h>
#include <getopt.h>
#include <stdlib.h>
#include <mincrypt.h>

typedef struct {
	char *name;
	uint64_t size;
	uint64_t compressed_size;
	uint32_t crc;
	bool encrypted;
} tArchiveFile;

int gBufferSize;

#if 0
#define UINT32STR(var, val)	\
	var[0] = (val >> 24) & 0xff;	\
	var[1] = (val >> 16) & 0xff;	\
	var[2] = (val >>  8) & 0xff;	\
	var[3] = (val      ) & 0xff;

#define UINT64STR(var, val)	\
	var[0] = (val >> 56) & 0xff;	\
	var[1] = (val >> 48) & 0xff;	\
	var[2] = (val >> 40) & 0xff;	\
	var[3] = (val >> 32) & 0xff;	\
	var[4] = (val >> 24) & 0xff;	\
	var[5] = (val >> 16) & 0xff;	\
	var[6] = (val >>  8) & 0xff;	\
	var[7] = (val      ) & 0xff;
#define BYTESTR(var, val)	\
	var[0] =  val;

#define WORDSTR(var, val)	\
	var[0] = (val >> 8) & 0xff;	\
	var[1] = (val     ) & 0xff;

#define GETBYTE(var)    (var[0])
#define GETWORD(var)    ((var[0] << 8) + (var[1]))
#define GETUINT32(var)	((var[0] << 24) + (var[1] << 16) + (var[2] << 8) + (var[3]))
#define GETUINT64(var)	(((uint64_t)var[0] << 56) + ((uint64_t)var[1] << 48) + ((uint64_t)var[2] << 40) + \
			((uint64_t)var[3] << 32) + ((uint64_t)var[4] << 24) + ((uint64_t)var[5] << 16)  + \
			((uint64_t)var[6] << 8) + (uint64_t)var[7])
#endif

/* archive.c */
void archive_generate_random_file(char *filename, int len);
int archive_encryption_test(char *salt, char *password);
void archive_encryption_enable(char *salt, char *password);
void archive_encryption_disable(void);
long get_file_size(char *fn);
char *get_random_file_name(void);
int archive_addFileToList(tArchiveFile *files, char *fn, long size, char *tmpDir);
tArchiveFile *archive_compile_list(char **infiles, int numFiles, char *tmpDir);
int archive_write_header(int fd, tArchiveFile *files, int numFiles);
uint64_t archive_fetch_data(int fd, int size);
tArchiveFile *archive_read_header(int fd, unsigned int *oNumFiles, int *oHdrSize);
int archive_write_file(int fd, char *filename, uint64_t size);
int archive_save(tArchiveFile *files, int numFiles, char *filename);
void archive_temp_cleanup(tArchiveFile *files, int numFiles);
tArchiveFile *archive_files(char *filename, unsigned int *oNumFiles);
int archive_extract(char *archiveFile, char *filename, char *dir);

/* compress.c */
unsigned long long physmem(void);
lzma_options_lzma *options(int preset, int extreme);
int xz_process_data(char *inputFile, char *outputFile, int decompress, unsigned int chunk_size, int overwrite, float *compression,
                        unsigned long long *origSize, unsigned long long *newSize);

/* crc.c */
uint32_t crc32_block(unsigned char *block, uint32_t length, uint64_t initVal);
uint32_t crc32_file(char *filename, int chunkSize);

/* main.c */
char *get_datetime(void);
int createArchive(char *filename, char *input);
int listArchive(char *filename);
int extractArchive(char *arcFile, char *filename, char *directory);

#endif
