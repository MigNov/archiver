# Library form
lib_LTLIBRARIES = libarchiver.la
libarchiver_la_CFLAGS = -Wall -fPIC -g -shared -DLIBARCHIVER
libarchiver_la_SOURCES = main.c crc.c compress.c archive.c archiver.h
libarchiver_la_LIBS = -llzma -lmincrypt

# Standalone binary form
bin_PROGRAMS = archiver
arhiver_CFLAGS = -Wall -fPIC -g
archiver_SOURCES = main.c crc.c compress.c archive.c archiver.h
archiver_LIBS = -llzma -lmincrypt

EXTRA_DIST = archiver.h
