#include "archiver.h"

unsigned int flags	= 0;
char archiveFile[1024]	= { 0 };
char input[1024]	= { 0 };
char runCwd[1024]	= { 0 };
char directory[1024]	= ".";	// Defaults to current directory
char gPassword[1024]	= { 0 };
char gSalt[1024]	= { 0 };

int parseFlags(int argc, char * const argv[]) {
    int option_index = 0, c;
    unsigned int retVal = 0;
    struct stat buf;
    struct option long_options[] = {
        {"create", 1, 0, 'c'},
        {"append", 1, 0, 'a'},
        {"list", 1, 0, 'l'},
        {"extract", 1, 0, 'x'},
        {"file", 1, 0, 'f'},
        {"hex", 0, 0, 'h'},
        {"salt", 1, 0, 's'},
        {"password", 1, 0, 'p'},
        {"directory", 1, 0, 'd'},
	{"cwd", 1, 0, 'w'},
	{"buffer-size", 1, 0, 'b'},
        {0, 0, 0, 0}
    };

    while (1) {
        c = getopt_long(argc, argv, "x:a:l:c:f:d:w:h",
                   long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'c':	retVal |= FLAG_ARCHIVE_CREATE;
			strncpy(archiveFile, optarg, sizeof(archiveFile));
			break;
            case 'a':	retVal |= FLAG_ARCHIVE_APPEND;
			strncpy(archiveFile, optarg, sizeof(archiveFile));
			break;
            case 'l':	retVal |= FLAG_ARCHIVE_LIST;
			strncpy(archiveFile, optarg, sizeof(archiveFile));
			break;
            case 'x':	retVal |= FLAG_ARCHIVE_EXTRACT;
			strncpy(archiveFile, optarg, sizeof(archiveFile));
			break;
            case 'f':	strncpy(input, optarg, sizeof(input));
			break;
            case 'h':	retVal |= FLAG_OUTPUT_HEX;
			break;
            case 's':	retVal |= FLAG_ARCHIVE_SALT;
			strncpy(gSalt, optarg, sizeof(gSalt));
			break;
            case 'p':	retVal |= FLAG_ARCHIVE_PASSWORD;
			strncpy(gPassword, optarg, sizeof(gPassword));
			break;
	    case 'w':   stat(optarg, &buf);
			if (!S_ISDIR(buf.st_mode)) {
				fprintf(stderr, "Cannot access directory %s. Exiting...\n", optarg);
				exit(1);
			}
			getcwd(runCwd, sizeof(runCwd));
			chdir(optarg);
			break;
            case 'd':	strncpy(directory, optarg, sizeof(directory));
			break;
            case 'b':	gBufferSize = atoi(optarg);
			if (gBufferSize == 0)
				gBufferSize = BUFFER_SIZE;
			break;

            default:
			break;
        }
    }

    return retVal;
}

int createArchive(char *filename, char *input)
{
	char fn[256];
	long size = -1;
	int i, numFiles;
	tArchiveFile *files;
	char tmpDir[256] = { 0 };
	char **infiles = NULL;
	FILE *fp;

	snprintf(tmpDir, sizeof(tmpDir), "/tmp/archiver-%d", getpid());

	if ((strlen(input) == 0) || (strcmp(input, "stdin") == 0)) {
		fp = stdin;
		printf("Please enter input files: ");
	}
	else
		fp = fopen(input, "r");

	if (fp == NULL)
		return 1;

	numFiles = 0;
	infiles = malloc( sizeof(char *) );
	while (!feof(fp)) {
		memset(fn, 0, sizeof(fn));
		fgets(fn, sizeof(fn), fp);
		if (strlen(fn) > 0) {
			if (fn[strlen(fn) - 1] == '\n')
				fn[strlen(fn) - 1] = 0;

			if (get_file_size(fn) > 0) {
				infiles = (char **)realloc( infiles, (numFiles + 1) * sizeof(char *));
				infiles[numFiles] = (char *)malloc( (strlen(fn) + 1) * sizeof(char));
				memset(infiles[numFiles], 0, (strlen(fn) + 1) * sizeof(char));
				strncpy(infiles[numFiles], fn, strlen(fn));
				numFiles++;
				//nextFileNum = archive_addFileToList(files, fn, size, tmpDir);
			}
			else
				fprintf(stderr, "File %s %s. Ignoring ...\n", fn, (size < 0) ? "not found" : "length is 0");
		}
	}

	if (numFiles == 0)
		fprintf(stderr, "No valid files found!\n");

	files = archive_compile_list(infiles, numFiles, tmpDir);

	archive_save(files, -1, filename);
	archive_temp_cleanup(files, -1);

	free(files);

	if (fp != stdin)
		fclose(fp);

	return 0;
}

int listArchive(char *filename)
{
	tArchiveFile *files;
	unsigned int numFiles = 0, i = 0;

	files = (tArchiveFile *)archive_files(filename, &numFiles);
	if (numFiles == 0)
		return 1;
	if (files == NULL)
		return 2;

	printf("Files in the archive: %d\n", numFiles);
	for (i = 0; i < numFiles; i++) {
		printf("File %d:\n", i + 1);
		printf("\tName: %s\n", files[i].name);
		printf("\tCRC: 0x%"PRIx32"\n", files[i].crc);
		if (files[i].size == files[i].compressed_size) {
			if (flags & FLAG_OUTPUT_HEX)
				printf("\tSize: 0x%"PRIx64" bytes\n", files[i].size);
			else
				printf("\tSize: %"PRIi64" bytes\n", files[i].size);

			printf("\tCompressed size: Not compressed\n");
		}
		else {
			if (flags & FLAG_OUTPUT_HEX) {
				printf("\tSize: 0x%"PRIx64" bytes\n", files[i].size);
				printf("\tCompressed size: 0x%"PRIx64" bytes\n", files[i].compressed_size);
			}
			else {
				printf("\tSize: %"PRIi64" bytes\n", files[i].size);
				printf("\tCompressed size: %"PRIi64" bytes\n", files[i].compressed_size);
			}
			printf("\tCompression ratio: %.2f%%\n", 100 - 
				(files[i].compressed_size / ((float)files[i].size / 100)) );
		}
		printf("\tEncrypted: %s\n", files[i].encrypted ? "True" : "False");
		printf("\n");
	}
	free(files);

	return 0;
}

int extractArchive(char *arcFile, char *filename, char *directory)
{
	int err, ret = 0;
	tArchiveFile *files;
	unsigned int numFiles = 0, i = 0;

	files = (tArchiveFile *)archive_files(arcFile, &numFiles);
	if (numFiles == 0) {
		fprintf(stderr, "Incorrect number of files in the archive\n");
		return 1;
	}
	if (files == NULL) {
		fprintf(stderr, "Invalid list of files\n");
		return 2;
	}

	if (filename && (strlen(filename) == 0))
		filename = "<all>";

	fprintf(stderr, "Archive file: %s\n", arcFile);
	fprintf(stderr, "File to extract: %s\n", filename);
	fprintf(stderr, "Destination directory: %s\n", directory);

	mkdir(directory, 0755);
	for (i = 0; i < numFiles; i++) {
		if ((strcmp(files[i].name, filename) == 0) ||
			(strcmp(filename, "<all>") == 0)) {
			if ((files[i].encrypted) && (gPassword == NULL)) {
				char *tmp = NULL;
				tmp = getpass("Please enter password: ");
				strncpy(gPassword, tmp, sizeof(gPassword));

				if ((strlen(gSalt) > 0) && (strlen(gPassword) > 0))
					archive_encryption_enable(gSalt, gPassword);
			}

			if ((err = archive_extract(arcFile, files[i].name, directory)) < 0) {
				printf("File %s extraction failed with code %d\n", files[i].name, err);
				ret = 1;
			}
			else
				printf("File %s has been extracted\n", files[i].name);
		}
	}
	free(files);

	return ret;
}

void usage(char *name)
{
	printf("Syntax: %s [--create|--append|--list|--extract] [--hex] [--buffer-size size] [--cwd] [--salt] [--password] archiveFile params\n\n", name);
	printf("Where params can be --file to specify input type (filename or 'stdin') for create and append or --file can also mean the\n");
	printf("name of the file to be extracted from the achive for --extract. You can also pass --directory parameter for extraction to\n");
	printf("specify the directory where file(s) should be extracted. If you pass --hex option the sizes will be output in hexadecimal\n");
	printf("format. If you specify the buffer size all the operations will be using the buffer size specified instead of default value\n");
	printf("which is %d bytes. You can also password-protect your archive by passing --salt and/or --password option. This will ask you\n", BUFFER_SIZE);
	printf("for your salt value or password on the standard input using getpass() C function (salt value is being used for the password\n");
	printf("initialization vector generation to improve security). You can change current working directory using the --cwd option.\n\n");
}

void restoreOnExit(void)
{
	if (strlen(runCwd) > 0)
		chdir(runCwd);
}

int main(int argc, char *argv[])
{
	int ret			= 1;
	char *tmp		= NULL;

	/* Preset default buffer size */
	gBufferSize = BUFFER_SIZE;
	strncpy(gSalt, DEFAULT_SALT_VAL, sizeof(gSalt));

	if (directory == NULL)
		strncpy(directory, ".", sizeof(directory));

	atexit(restoreOnExit);
	flags = parseFlags(argc, argv);

	if ((!(flags & FLAG_ARCHIVE_SALT)) && (flags & FLAG_ARCHIVE_PASSWORD) && (strlen(gSalt) == 0)) {
		tmp = getpass("Please enter salt value: ");
		if (tmp != NULL)
			strncpy(gSalt, tmp, sizeof(gSalt));
	}

	if ((flags & FLAG_ARCHIVE_PASSWORD) && (strlen(gPassword) == 0)) {
		tmp = getpass("Please enter password: ");
		if (tmp != NULL)
			strncpy(gPassword, tmp, sizeof(gPassword));
	}

	if ((flags & FLAG_ARCHIVE_PASSWORD) && (strlen(gSalt) > 0) && (strlen(gPassword) > 0)) {
		archive_encryption_enable(gSalt, gPassword);
	}

	if (flags & FLAG_ARCHIVE_CREATE)
		ret = createArchive(archiveFile, input);
	else
	if (flags & FLAG_ARCHIVE_APPEND) {
		fprintf(stderr, "Appending files is not supported yet!\n");
		ret = 1;
	}
	else
	if (flags & FLAG_ARCHIVE_LIST)
		ret = listArchive(archiveFile);
	else
	if (flags & FLAG_ARCHIVE_EXTRACT)
		ret = extractArchive(archiveFile, input, directory);
	else
		usage(argv[0]);

	return ret;
}

