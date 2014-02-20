//#define DEBUG_ARCHIVE

#include "archiver.h"

#ifdef DEBUG_ARCHIVE
#define DPRINTF(fmt, ...) \
do { char *dtmp = get_datetime(); fprintf(stderr, "[%s ", dtmp); free(dtmp); dtmp=NULL; fprintf(stderr, "archiver/archive  ] " fmt , ## __VA_ARGS__); fflush(stderr); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

int _encryption		= 0;
int _fileNum		= 0;
char tempDir[1024]	= { 0 };

void archive_generate_random_file(char *filename, int len)
{
	int i;
	FILE *fp = fopen(filename, "w");

	srand( time(NULL) );
	for (i = 0; i < len; i++)
		fputc( (rand() % 256), fp);

	fclose(fp);
	DPRINTF("%s: Generated %s of %d random bytes\n", __FUNCTION__, filename, len);
}

int archive_encryption_test(char *salt, char *password)
{
	char filename[] = "/tmp/archiver-mincrypt-test-XXXXXX";
	char filename2[] = "/tmp/archiver-mincrypt-test-out-XXXXXX";

	if ((salt == NULL) || (password == NULL))
		return -EINVAL;

	mkstemp(filename);
	mkstemp(filename2);

	archive_generate_random_file(filename, 524288);

	mincrypt_set_password(salt, password, VECTOR_MULTIPLIER);
	if (mincrypt_encrypt_file(filename, filename2, NULL, NULL, VECTOR_MULTIPLIER) < 0)
		return -EIO;

	mincrypt_set_password(salt, password, VECTOR_MULTIPLIER);
	if (mincrypt_decrypt_file(filename2, filename, NULL, NULL, VECTOR_MULTIPLIER) < 0)
		return -EIO;

	unlink(filename2);
	unlink(filename);

	return 0;
	
}

void archive_encryption_enable(char *salt, char *password)
{
	if ((salt == NULL) || (password == NULL))
		return;

	if (archive_encryption_test(salt, password) < 0) {
		DPRINTF("%s: Test failed. Cannot enable compression\n", __FUNCTION__);
		return;
	}

	if ((strlen(salt) > 0) && (strlen(password) > 0)) {
		DPRINTF("Enabling encryption\n");
		_encryption = 1;
		mincrypt_set_password(salt, password, VECTOR_MULTIPLIER);
	}
	else
		fprintf(stderr, "Error: Invalid salt or password, cannot enable encryption.\n");
}

void archive_encryption_disable(void)
{
	DPRINTF("Disabling encryption\n");
	_encryption = 0;
	mincrypt_cleanup();
}

long get_file_size(char *fn)
{
	int fd;
	long ret;

	fd = open(fn, O_RDONLY);
	if (fd < 0)
		return -1;
	ret = lseek(fd, 0, SEEK_END);
	close(fd);

	return ret;
}

char *get_random_file_name(void)
{
	char filename[] = "/tmp/archiver-XXXXXX";

	mkstemp(filename);
	return strdup(filename);
}

int archive_addFileToList(tArchiveFile *files, char *fn, long size, char *tmpDir)
{
	char outFile[256] = { 0 };
	int err;
	unsigned long long origSize, newSize;
	float compression;

	if (fn == NULL)
		return _fileNum;

	if (size == 0)
		size = get_file_size(fn);

	if ((strlen(tempDir) == 0) && (tmpDir != NULL)) {
		strncpy(tempDir, tmpDir, sizeof(tempDir));
		mkdir(tmpDir, 0755);
	}

	DPRINTF("Creating directory %s\n", tmpDir);
	mkdir(tmpDir, 0755);

	snprintf(outFile, sizeof(outFile), "%s/%s", tmpDir, basename(fn));
	DPRINTF("Setting up outFile to %s\n", outFile);

	if (_encryption) {
		int fd;
		char *tmpFn = get_random_file_name();
		if ((err = xz_process_data(fn, tmpFn, 0, gBufferSize, 1, &compression, &origSize, &newSize))) {
			fprintf(stderr, "Compression of %s failed with error %d!\n", basename(fn), err);
			return _fileNum;
		}
		if (mincrypt_encrypt_file(tmpFn, outFile, NULL, NULL, VECTOR_MULTIPLIER) != 0)
			fprintf(stderr, "Encryption failed!\n");
		unlink(tmpFn);

		fd = open(outFile, O_RDONLY | O_LFILE);
		newSize = lseek(fd, 0, SEEK_END);
		close(fd);

		DPRINTF("%s: New file size is %lld bytes (%s)\n", __FUNCTION__, newSize, outFile);
	}
	else
		if ((err = xz_process_data(fn, outFile, 0, gBufferSize, 1, &compression, &origSize, &newSize))) {
			fprintf(stderr, "Compression of %s failed with error %d!\n", basename(fn), err);
			return _fileNum;
		}

	DPRINTF("Compression ratio: %.2f%% (original size = %lld bytes, new size = %lld bytes, fn = %s)\n",
			compression, origSize, newSize, basename(fn));

	if (newSize >= origSize) {
		DPRINTF("New file %s is bigger or of the same size like original. Disabling compression...\n", outFile);
		rename(fn, outFile);
		newSize = origSize;
	}

	DPRINTF("%s: Adding file #%d: %s\n", __FUNCTION__, _fileNum + 1, fn);

	files[_fileNum].name = strdup( basename(fn) );
	files[_fileNum].size = origSize;
	files[_fileNum].compressed_size = newSize;
	files[_fileNum].crc = crc32_file(fn, gBufferSize);
	files[_fileNum].encrypted = _encryption ? true : false;

	_fileNum++;

	return _fileNum;
}

tArchiveFile *archive_compile_list(char **infiles, int numFiles, char *tmpDir)
{
	int i;
	tArchiveFile *tmpFiles = NULL;

	tmpFiles = (tArchiveFile *)malloc( numFiles * sizeof(tArchiveFile) );

	for (i = 0; i < numFiles; i++)
		archive_addFileToList(tmpFiles, infiles[i], 0, tmpDir);

	return tmpFiles;
}

int archive_write_header(int fd, tArchiveFile *files, int numFiles)
{
	int i, ret = 0;
	char *name = NULL;
	unsigned char dataByte[1] = { 0 }, dataWord[2] = { 0 };
	unsigned char data32[4] = { 0 }, data64[8] = { 0 };

	write(fd, SIGNATURE, sizeof(SIGNATURE));
	ret = sizeof(SIGNATURE);

	WORDSTR(dataWord, ARCHIVER_VERSION);
	write(fd, dataWord, sizeof(dataWord));
	ret += sizeof(dataWord);

	/* Extra header size */
	WORDSTR(dataWord, 0);
	write(fd, dataWord, sizeof(dataWord));
	ret += sizeof(dataWord);

	WORDSTR(dataWord, numFiles);
	write(fd, dataWord, sizeof(dataWord));
	ret += sizeof(dataWord);

	WORDSTR(dataWord, 0x0000);
	write(fd, dataWord, sizeof(dataWord));
	ret += sizeof(dataWord);

	for (i = 0; i < numFiles; i++) {
		BYTESTR(dataByte, strlen(files[i].name));
		write(fd, dataByte, sizeof(dataByte));
		ret += sizeof(dataByte);

		name = strdup( basename(files[i].name) );
		write(fd, name, strlen(name));
		ret += strlen(name);
		free(name);

		UINT32STR(data32, files[i].crc);
		write(fd, data32, sizeof(data32));
		ret += sizeof(data32);

		UINT64STR(data64, files[i].size);
		write(fd, data64, sizeof(data64));
		ret += sizeof(data64);

		UINT64STR(data64, files[i].compressed_size);
		write(fd, data64, sizeof(data64));
		ret += sizeof(data64);

		BYTESTR(dataByte, files[i].encrypted ? 1 : 0);
		write(fd, dataByte, sizeof(dataByte));
		ret += sizeof(dataByte);

		WORDSTR(dataWord, 0x4e45);
		write(fd, dataWord, sizeof(dataWord));
		ret += sizeof(dataWord);
	}
	WORDSTR(dataWord, 0x4e46);
	write(fd, dataWord, sizeof(dataWord));
	ret += sizeof(dataWord);

	return ret;
}

uint64_t archive_fetch_data(int fd, int size)
{
	unsigned char data[8] = { 0 };
	uint64_t val = 0;

	if (read(fd, data, size) != size) {
		DPRINTF("Invalid data block\n");
		return (uint64_t)-1;
	}

	switch (size) {
		case 1:	val = GETBYTE(data);
			break;
		case 2:	val = GETWORD(data);
			break;
		case 4:	val = GETUINT32(data);
			break;
		case 8: val = GETUINT64(data);
			break;
		default:val = (uint64_t)-1;
			DPRINTF("Cannot get %d bytes: Not implemented\n", size);
			break;
	}

	return val;
}

tArchiveFile *archive_read_header(int fd, unsigned int *oNumFiles, int *oHdrSize)
{
	uint32_t crc = 0;
	uint64_t size = 0, csize = 0;
	char name[256] = { 0 };
	unsigned int numFiles = 0, hdrSize = 0, nameLen = 0;
	unsigned int i = 0, off = 0, tmp = 0, encrypted = 0;
	unsigned int version = 0, extraHdrSize = 0;
	tArchiveFile *files = NULL;

	if (archive_fetch_data(fd, 4) != GETUINT32(SIGNATURE))
		return NULL;

	DPRINTF("Signature OK\n");

	if ((version = archive_fetch_data(fd, 2)) == (uint64_t)-1)
		return NULL;

	DPRINTF("Archive version: %d\n", version);

	if ((extraHdrSize = archive_fetch_data(fd, 2)) == (uint64_t)-1)
		return NULL;

	if ((numFiles = archive_fetch_data(fd, 2)) == (uint64_t)-1)
		return NULL;

	DPRINTF("Found %d files in archive\n", numFiles);

	if (lseek(fd, 0, SEEK_CUR) != HEADER_SIZE_OFFSET) {
		DPRINTF("Header size offset is not found where expected.!\n");
		return NULL;
	}

	if ((hdrSize = archive_fetch_data(fd, 2)) == (uint64_t)-1)
		return NULL;

	DPRINTF("Basic header size: %d (0x%x) bytes\n", hdrSize, hdrSize);
	DPRINTF("Extra header size: %d (0x%x) bytes\n", extraHdrSize, extraHdrSize);
	DPRINTF("Total header size: %d (0x%x) bytes\n", hdrSize + extraHdrSize, hdrSize + extraHdrSize);

	if (oHdrSize != NULL)
		*oHdrSize = hdrSize + extraHdrSize;

	files = (tArchiveFile *)malloc( numFiles * sizeof(tArchiveFile) );
	memset(files, 0, numFiles * sizeof(tArchiveFile) );

	for (i = 0; i < numFiles; i++) {
		memset(name, 0, sizeof(name));
		if ((nameLen = archive_fetch_data(fd, 1)) == (uint64_t)-1)
			goto outfree;

		if (read(fd, name, nameLen) != nameLen)
			goto outfree;

		if ((crc = archive_fetch_data(fd, 4)) == (uint64_t)-1)
			goto outfree;

		if ((size = archive_fetch_data(fd, 8)) == (uint64_t)-1)
			goto outfree;

		if ((csize = archive_fetch_data(fd, 8)) == (uint64_t)-1)
			goto outfree;

		if ((encrypted = archive_fetch_data(fd, 1)) == (uint64_t)-1)
			goto outfree;

		if ((tmp = archive_fetch_data(fd, 2)) == (uint64_t)-1)
			goto outfree;

		if (tmp != 0x4e45) {
			DPRINTF("Premature end of headers\n");
			goto outfree;
		}

		files[i].name = strdup(name);
		files[i].crc = crc;
		files[i].size = size;
		files[i].compressed_size = csize;
		files[i].encrypted = (encrypted ? true : false);

		DPRINTF("Name: %s, CRC: 0x%" PRIx32", size: %"PRIi64", csize: %"PRIi64", encrypted: %d\n",
			name, crc, size, csize, encrypted);
	}

	if ((tmp = archive_fetch_data(fd, 2)) == (uint64_t)-1)
		goto outfree;

	if (tmp != 0x4e46) {
		DPRINTF("Premature end of headers\n");
		goto outfree;
	}

	if ((off = lseek(fd, 0, SEEK_CUR)) != hdrSize) {
		DPRINTF("Data should be starting on offset %d but found on offset %d\n", hdrSize, off);
		goto outfree;
	}

	if (oNumFiles != NULL)
		*oNumFiles = numFiles;

	DPRINTF("Headers read successfully for %d files\n", numFiles);
	return files;

outfree:
	free(files);

	return NULL;
}

int archive_write_file(int fd, char *filename, uint64_t size)
{
	int fdIn, rc;
	char fn[1024] = { 0 };
	char buf[BUFFER_SIZE];

	snprintf(fn, sizeof(fn), "%s/%s", tempDir, filename);

	fdIn = open(fn, O_RDONLY | O_LFILE);
	if (fdIn < 0) {
		int errno_saved = errno;

		DPRINTF("Error opening input file %s, error code %d (%s)\n", fn, errno_saved, strerror(errno_saved));
		return -errno_saved;
	}

	while ((rc = read(fdIn, buf, BUFFER_SIZE)) > 0) {
		write(fd, buf, rc);
	}

	DPRINTF("File %s written into the archive\n", fn);

	return 0;
}

int archive_save(tArchiveFile *files, int numFiles, char *filename)
{
	int fd, len;
	unsigned int i;

	if (numFiles < 0)
		numFiles = _fileNum;

	fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (fd < 0) {
		int errno_saved = errno;

		DPRINTF("Cannot create archive file %s, error %d (%s)\n", filename, errno_saved, strerror(errno_saved));
		return -errno_saved;
	}

	len = archive_write_header(fd, files, numFiles);
	DPRINTF("Number of files is %d\n", numFiles);
	DPRINTF("Archive header size: %d (0x%x) bytes\n", len, len);

	for (i = 0; i < numFiles; i++)
		archive_write_file(fd, basename(files[i].name), files[i].compressed_size);

	/* Now alter the header size */
	unsigned char dataWord[2] = { 0 };
	lseek(fd, HEADER_SIZE_OFFSET, SEEK_SET);
	WORDSTR(dataWord, len);
	write(fd, dataWord, sizeof(dataWord));

	close(fd);

	return 0;
}

void archive_temp_cleanup(tArchiveFile *files, int numFiles)
{
	char fn[1024] = { 0 };
	int i;

	if (numFiles < 0)
		numFiles = _fileNum;

	for (i = 0; i < numFiles; i++) {
		snprintf(fn, sizeof(fn), "%s/%s", tempDir, files[i].name);
		unlink(fn);
	}

	rmdir(tempDir);
}

tArchiveFile *archive_files(char *filename, unsigned int *oNumFiles)
{
	int fd;
	unsigned int numFiles = 0;
	tArchiveFile *files = NULL;

	fd = open(filename, O_RDONLY | O_LFILE);
	if (fd < 0) {
		DPRINTF("Cannot open archive file %s, error code %d (%s)\n", filename, errno, strerror(errno));
		return NULL;
	}

	files = archive_read_header(fd, &numFiles, NULL);
	if (files == NULL) {
		DPRINTF("Cannot read header for %s\n", filename);
		return NULL;
	}

	close(fd);

	if (oNumFiles != NULL)
		*oNumFiles = numFiles;

	return files;
}

int archive_extract(char *archiveFile, char *filename, char *dir)
{
	char buf[BUFFER_SIZE] = { 0 }, fn[1024] = { 0 };
	uint64_t filePos = 0, fileSize = 0, originalSize = 0, remaining;
	uint32_t crc = 0;
	int fd, fdOut, i, rc;
	unsigned int numFiles = 0;
	int hdrSize = 0, readlen = 0, ret = 0;
	bool compressed = false, encrypted = false;
	tArchiveFile *files = NULL;

	if (gBufferSize == 0)
		gBufferSize = BUFFER_SIZE;

	if ((filename == NULL) || (strlen(filename) == 0)) {
		DPRINTF("Filename for extraction is not specified\n");
		return -EIO;
	}

	if ((dir == NULL) || (strlen(dir) == 0)) {
		DPRINTF("Directory for extraction is not specified\n");
		return -EIO;
	}

	if ((dir[0] != '.') && (access(dir, F_OK) == 0)) {
		struct stat buf;

		stat(dir, &buf);
		if (!S_ISDIR(buf.st_mode)) {
			DPRINTF("File system object %s is not a directory\n", dir);
			return -EEXIST;
		}
	}

	fd = open(archiveFile, O_RDONLY | O_LFILE);
	if (fd < 0) {
		int errno_saved = errno;
		DPRINTF("Cannot open archive file %s, error code %d (%s)\n", filename, errno_saved, strerror(errno_saved));
		return -errno_saved;
	}

	files = archive_read_header(fd, &numFiles, &hdrSize);
	if (files == NULL)
		return -EIO;

	if (numFiles == 0)
		return 0;

	filePos = hdrSize;
	for (i = 0; i < numFiles; i++) {
		if (strcmp(files[i].name, filename) == 0) {
			compressed = (files[i].compressed_size != files[i].size);
			crc = files[i].crc;
			encrypted = files[i].encrypted ? true : false;
			originalSize = files[i].size;
			fileSize = files[i].compressed_size;
			break;
		}

		filePos += files[i].compressed_size;
	}

	free(files);

	lseek(fd, filePos, SEEK_SET);
	if (compressed) {
		snprintf(tempDir, sizeof(tempDir), "/tmp/archiver-%d", getpid());
		mkdir(tempDir, 0755);
		snprintf(fn, sizeof(fn), "%s/%s", tempDir, filename);
	}
	else
		snprintf(fn, sizeof(fn), "%s/%s", dir, filename);

	fdOut = open(fn, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC | O_LFILE, 0644);
	if (fdOut < 0) {
		int errno_saved = errno;
		DPRINTF("Cannot open output file %s, error code %d (%s)\n", filename, errno_saved, strerror(errno_saved));
		close(fd);
		return -errno_saved;
	}

	remaining = fileSize;
	readlen = (fileSize > BUFFER_SIZE) ? BUFFER_SIZE : fileSize;
	while ((remaining > 0) && ((rc = read(fd, buf, readlen)) > 0)) {
		if (write(fdOut, buf, rc) != rc) {
			close(fd);
			close(fdOut);
			DPRINTF("Cannot write data to %s\n", fn);
			return -EIO;
		}
		remaining -= readlen;
		readlen = (fileSize > BUFFER_SIZE) ? BUFFER_SIZE : fileSize;
	}

	if ((lseek(fdOut, 0, SEEK_CUR) != fileSize) && (!compressed && !encrypted)) {
		DPRINTF("Invalid output file size (expected %"PRIi64", is %"PRIi64")\n", fileSize, lseek(fdOut, 0, SEEK_CUR));
		return -EIO;
	}

	close(fd);
	close(fdOut);

	if (compressed) {
		int err;
		char outFile[1024] = { 0 };
		float compression;
		unsigned long long origSize, newSize;

		snprintf(outFile, sizeof(outFile), "%s/%s", dir, filename);

		if (encrypted) {
			char *tmpFn = get_random_file_name();
			DPRINTF("%s: Decrypting ...\n", __FUNCTION__);
			if (mincrypt_decrypt_file(fn, tmpFn, NULL, NULL, VECTOR_MULTIPLIER) != 0) {
				fprintf(stderr, "Decryption failed!\n");
				ret = -EIO;
			}
			if ((ret == 0) && (err = xz_process_data(tmpFn, outFile, 1, gBufferSize, 1, &compression, &origSize, &newSize))) {
				fprintf(stderr, "Decompression of %s failed with error %d!\n", basename(fn), err);
				ret = -EIO;
			}
			unlink(tmpFn);
		}
		else
			if ((err = xz_process_data(fn, outFile, 1, gBufferSize, 1, &compression, &origSize, &newSize))) {
				fprintf(stderr, "Decompression of %s failed with error %d!\n", basename(fn), err);
				ret = -EIO;
			}

		unlink(fn);
		rmdir(tempDir);

		/* Ensure we won't exceed file size since read *may* be done by big chunks */
		truncate(outFile, originalSize);

		if (crc32_file(outFile, gBufferSize) != crc) {
			DPRINTF("CRC is not valid (expected 0x%"PRIx32", found 0x%"PRIx32"; on compressed)\n",
					crc, crc32_file(outFile, gBufferSize));
			ret = -EINVAL;
			//unlink(outFile);
		}

		if (ret == 0)
			DPRINTF("Extraction of %s done successfully\n", filename);
		else
			DPRINTF("Extraction error code %d\n", ret);
	}
	else {
		if (encrypted) {
			char *tmpFn = get_random_file_name();
			if (mincrypt_decrypt_file(fn, tmpFn, "test", "test", VECTOR_MULTIPLIER) != 0) {
				fprintf(stderr, "Decryption failed!\n");
				ret = -EIO;
			}
			else
				rename(tmpFn, fn);

			DPRINTF("fn => '%s'\n", fn);
			if (ret == 0)
				DPRINTF("Extraction of %s done successfully\n", filename);
			else
				DPRINTF("Extraction error code %d\n", ret);
		}

		if ((crc32_file(fn, gBufferSize) != crc) || (fileSize == 0)) {
			DPRINTF("CRC is not valid (expected 0x%"PRIx32", found 0x%"PRIx32"; on non-compressed)\n",
					crc, crc32_file(fn, gBufferSize));
			ret = -EINVAL;
			//unlink(fn);
		}
		else
			DPRINTF("Extraction of done %s successfully\n", filename);
	}

	return ret;
}

