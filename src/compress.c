//#define TEST_LZMA
//#define DEBUG_LZMA

#include "archiver.h"

#ifdef DEBUG_LZMA
#define DPRINTF(fmt, ...) \
do { char *dtmp = get_datetime(); fprintf(stderr, "[%s ", dtmp); free(dtmp); dtmp=NULL; fprintf(stderr, "archiver/compress ] " fmt , ## __VA_ARGS__); fflush(stderr); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

unsigned long long physmem(void)
{
	unsigned long long ret = 0;
	const long pagesize = sysconf(_SC_PAGESIZE);
	const long pages = sysconf(_SC_PHYS_PAGES);

	if ( (pagesize != -1) || (pages != -1) )
		ret = (unsigned long long)(pagesize) * (unsigned long long)(pages);

	return ret;
}

lzma_options_lzma *options(int preset, int extreme)
{
	uint32_t upreset = preset;

	if (extreme)
		upreset |= LZMA_PRESET_EXTREME;

	lzma_options_lzma *options = malloc(sizeof(lzma_options_lzma));
		*options = (lzma_options_lzma){
		.dict_size = LZMA_DICT_SIZE_DEFAULT,
		.preset_dict =  NULL,
		.preset_dict_size = 0,
		.lc = LZMA_LC_DEFAULT,
		.lp = LZMA_LP_DEFAULT,
		.pb = LZMA_PB_DEFAULT,
		.mode = LZMA_MODE_NORMAL,
		.nice_len = 64,
		.mf = LZMA_MF_BT4,
		.depth = 0,
	};
	if (lzma_lzma_preset(options, upreset)) {
		fprintf(stderr, "LZMA: Error in setting up preset\n");
		return NULL;
	}

	return options;
}

int xz_process_data(char *inputFile, char *outputFile, int decompress, unsigned int chunk_size, int overwrite, float *compression,
			unsigned long long *origSize, unsigned long long *newSize)
{
	lzma_stream stream = LZMA_STREAM_INIT;
	lzma_ret ret;
	lzma_action action = LZMA_RUN;
	static lzma_filter filters[2];
	static uint8_t *in_buf, *out_buf;
	int retval = -1, i;
	int in_fd, out_fd, num, end;
	unsigned long long fileSize;
	time_t startTime;

	if (chunk_size == 0)
		chunk_size = BUFFER_SIZE;

	DPRINTF("xz_process_data('%s', '%s', %d, %d, %d) called\n", inputFile, outputFile,
		decompress, chunk_size, overwrite);

	if ((access(outputFile, F_OK) == 0) && (!overwrite)) {
		DPRINTF("File %s exists and overwrite not enabled\n", outputFile);
		return 1;
	}

	in_buf = malloc( chunk_size * sizeof(uint8_t) );
	if (in_buf == NULL) {
		DPRINTF("Not enough memory for input buffer\n");
		return -ENOMEM;
	}
	out_buf = malloc( chunk_size * sizeof(uint8_t) );
	if (out_buf == NULL) {
		free(in_buf);
		DPRINTF("Not enough memory for output buffer\n");
		return -ENOMEM;
	}

	if (decompress) {
		DPRINTF("Initializing decompressor\n");
		ret = lzma_stream_decoder(&stream, physmem() / 3,
			LZMA_TELL_UNSUPPORTED_CHECK | LZMA_CONCATENATED);
	}
	else {
		DPRINTF("Initializing compressor\n");
		lzma_check check = LZMA_CHECK_CRC32;
		filters[0].id = LZMA_FILTER_LZMA2;
		filters[0].options = options(COMPRESSION_LEVEL, 0);
		filters[1].id = LZMA_VLI_UNKNOWN;

		ret =  lzma_stream_encoder(&stream, filters, check);
	}

	if ( ret != LZMA_OK )
	{
		DPRINTF("Failed to init lzma stream %scoder (%d)\n",
                          decompress ? "de" : "en", (int)ret);
		free(in_buf);
		free(out_buf);
		return -EIO;
	}

	DPRINTF("Opening input file %s\n", inputFile);
	in_fd = open(inputFile, O_RDONLY | O_SYNC | O_LFILE);
	if (in_fd < 0) {
		DPRINTF("Cannot open %s for input\n", inputFile);
		free(in_buf);
		free(out_buf);
		return -EEXIST;
	}

	fileSize = (unsigned long long)lseek64(in_fd, 0, SEEK_END);
	lseek64(in_fd, 0, SEEK_SET);

	out_fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC | O_LFILE, 0644);
	if (out_fd < 0) {
		DPRINTF("Cannot open output file %s, error %d (%s)\n", outputFile, errno, strerror(errno));
		free(in_buf);
		free(out_buf);
		close(in_fd);
		return -EIO;
	}

	end = 0;
	stream.next_in = in_buf;
	if ((num = read(in_fd, in_buf, chunk_size)) < 0)
		end = 1;
	DPRINTF("Buffer size is: %d bytes\n", num);
	stream.avail_in = num;

	stream.next_out = out_buf;
	stream.avail_out = chunk_size;

	for ( ; ; )
	{
		if ( (stream.avail_in == 0) || end )
		{
			if ((num = read(in_fd, in_buf, chunk_size)) <= 0) {
				DPRINTF("Setting up end = 1\n");
				end = 1;
			}
		DPRINTF("Read new stream: %d bytes\n", num);
		stream.next_in = in_buf;
		stream.avail_in = num;

		if (end)
			action = LZMA_FINISH;
		}
		ret = lzma_code(&stream, action);

		if ( stream.avail_out == 0 )
		{
			DPRINTF("Flushing buffer: %d bytes\n", chunk_size - stream.avail_out);
			if (write(out_fd, out_buf, chunk_size - stream.avail_out) < 0)
				DPRINTF("Error when writing: %d\n", errno);

			memset(out_buf, 0, chunk_size);
			stream.next_out = out_buf;
			stream.avail_out = chunk_size;
		}

		if (ret != LZMA_OK)
		{
			int stop = ret != LZMA_NO_CHECK
				&& ret != LZMA_UNSUPPORTED_CHECK;

			if (stop) {
				DPRINTF("About to write %d bytes\n", chunk_size - stream.avail_out);
				if (stream.avail_out < chunk_size)
					DPRINTF("Stream.avail_out: %d, IO_BUFFER_SIZE: %d\n",
						stream.avail_out, chunk_size);
				if (write(out_fd, out_buf, chunk_size - stream.avail_out) < 0)
					DPRINTF("Error when writing to output file: %d\n", errno);
				retval = 0;
				break;
			}

			if (ret == LZMA_STREAM_END) {
				if (stream.avail_in == 0 && !end) {
					memset(in_buf, 0, chunk_size);
					if ((num = read(in_fd, in_buf, chunk_size)) <= 0)
						end = 1;
					DPRINTF("Read: %d bytes\n", num);
					stream.next_in = in_buf;
					stream.avail_in = num;
				}
			}
		}
	}

	if (origSize != NULL)
		*origSize = stream.total_in;
	if (newSize != NULL)
		*newSize = stream.total_out;

	free(in_buf);
	free(out_buf);
	close(out_fd);
	close(in_fd);
	lzma_end(&stream);

	if ((origSize != NULL) && (newSize != NULL))
	DPRINTF("LZMA %scompression done, original size = %lld, newSize = %lld (%.2f%%)\n",
		decompress ? "de" : "", *origSize, *newSize, 100 - (*newSize / ((float)*origSize / 100)));

	if ((origSize != NULL) && (newSize != NULL) && (compression != NULL))
		*compression = 100 - (*newSize / ((float)*origSize / 100));

	return retval;
}

#ifdef TEST_LZMA
int generateRandomBlock(char *filename, int size)
{
	int		fd, i, ii, val;
	unsigned char	data[4096] = { 0 };
	int		table[4096] = { 0 };
	int		freq[256]	= { 0 };

	fd = open(filename, O_WRONLY | O_SYNC | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		DPRINTF("Random block generation error %d (%s)\n", errno, strerror(errno));
		return 0;
	}

	DPRINTF("Generating table of %d bytes\n", sizeof(table) / sizeof(table[0]));
	for (i = 0; i < (sizeof(table) / sizeof(table[0])); i++) {
		srandom(i * size * time(NULL));
		table[i] = random();
	}

	for (i = 0; i < size / (sizeof(table) / sizeof(table[0])); i++) {
		val = random() / (i + 1);
		for (ii = 0; ii < (sizeof(table) / sizeof(table[0])); ii++) {
			data[ii] = (val * table[ii]) % 256;
			freq[ data[ii] ]++;
		}
		write(fd, data, ii);
	}

	int x=0, max = 0, maxi = 0;
	for (i = 0; i < 256; i++)
		fprintf(stderr, "data[%d] => %d\n", i, data[i]);

	close(fd);
	return 1;
}

int diffIsSame(char *command)
{
        FILE *fp;
        char out[16] = { 0 };

        fp = popen(command, "r");
        if (!fp)
                return -1;
        fgets(out, sizeof(out), fp);
        fclose(fp);

        return (strlen(out) == 0);
}

int main()
{
	char			filename [256]	= { 0 };
	char			filename2[256]	= { 0 };
	char			filename3[256]	= { 0 };
	char			cmd[256]	= { 0 };
	int			ret		= 0;
	int			err		= 0;
	float			compression;
	unsigned long long	origSize;
	unsigned long long	newSize;

	snprintf(filename,  sizeof(filename),  "/tmp/testlzma-%d.tmp", getpid());
	snprintf(filename2, sizeof(filename2), "%s-lzma", filename);
	snprintf(filename3, sizeof(filename3), "%s-back", filename);

	if (!generateRandomBlock(filename, 1 << 20)) {
		fprintf(stderr, "Error: Cannot generate random block\n");
		ret = -1;
		goto end;
	}

	if ((err = xz_process_data(filename, filename2, 0, 16 * (1 << 10), 1, &compression, &origSize, &newSize)) != 0) {
		printf("xz_process_data returned error %d for compression\n", err);
		ret = -2;
		goto end;
	}

	printf("File compressed.   Compression ratio: %.2f%% (original size = %lld B, new size = %lld B)\n", compression, origSize, newSize);

	if ((err = xz_process_data(filename2, filename3, 1, 3 * (1 << 10), 1, &compression, &origSize, &newSize)) != 0) {
		printf("xz_process_data returned error %d for decompression\n", err);
		ret = -3;
		goto end;
	}

	printf("File decompressed. Compression ratio: %.2f%% (original size = %lld B, new size = %lld B)\n", compression, origSize, newSize);

	snprintf(cmd, sizeof(cmd), "diff -up %s %s", filename, filename3);
	if (!diffIsSame(cmd)) {
		fprintf(stderr, "Error: Uncompressed file and file after decompression are different\n");
		ret = -4;
		goto end;
	}

	snprintf(cmd, sizeof(cmd), "cp %s testxxx", filename);
	system(cmd);

	printf("Everything is OK\n");
end:
	unlink(filename);
	unlink(filename2);
	unlink(filename3);
	
	return ret;
}
#endif
