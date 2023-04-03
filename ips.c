#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define filelist(...) (FILE *[]) { __VA_ARGS__, NULL }

static const size_t LENGTH    = 1024;
static uint8_t buffer[LENGTH];

static const size_t HEADER    = 5;
static const size_t OFFSET    = 3;
static const size_t SIZE      = 2;

static const size_t RLE       = 0;
static const size_t RLE_SIZE  = 2;
static const size_t RLE_VALUE = 1;

void finish(int status, FILE ** toClose, FILE * stream, const char * resultsFormat, ...) {
    if (toClose) {
        while (*toClose) {
            fclose(*toClose);
            ++toClose;
        }
    }

    va_list args;
    va_start(args, resultsFormat);
    vfprintf(stream, resultsFormat, args);
    va_end(args);

    exit(status);
}

int copy(FILE * dst, FILE * src) {
    fseek(src, 0, SEEK_SET);
    fseek(dst, 0, SEEK_SET);

    while (!feof(src)) {
        size_t r = fread(buffer, sizeof(uint8_t), LENGTH, src);
        if (r < 0) {
            return 0;
        }

        size_t toWrite = r;
        size_t offset = 0;

        while (toWrite > 0) {
            size_t w = fwrite(buffer + offset, sizeof(uint8_t), toWrite, dst);

            if (w < 0) {
                return 0;
            }

            toWrite -= w;
            offset += w;
        }
    }

    return 1;
}

int main(int argc, char ** argv) {
    if (argc < 2 || argc > 4) {
        finish(1, NULL, stderr, "usage: %s <base.bin> <patch.ips> [patched.bin]\n", argv[0]);
    }

    const char * dst = argc == 4 ? argv[3] : argv[1];
    const char * mode = argc == 4 ? "wb" : "r+b";
    FILE * patched = fopen(dst, mode);
    if (!patched) {
        finish(1, NULL, stderr, "failed to open %s\n", dst);
    }

    if (argc == 4) {
        FILE * base = fopen(argv[1], "rb");

        if (!base) {
            finish(1, filelist(patched), stderr, "failed to open %s\n", argv[1]);
        }

        if (!copy(patched, base)) {
            finish(1, filelist(base, patched), stderr, "failed to copy %s into %s\n", argv[1], argv[3]);
        }

        fclose(base);
    }

    FILE * patch = fopen(argv[2], "rb");
    if (!patch) {
        finish(1, filelist(patched), stderr, "failed to open %s\n", argv[2]);
    }

    FILE ** toClose = filelist(patch, patched);

    size_t r = fread(buffer, sizeof(uint8_t), HEADER, patch);
    if (r != HEADER || strncmp((const char *) buffer, "PATCH", HEADER) != 0) {
        finish(1, toClose, stderr, "invalid patch header\n");
    }

    while (1) {
        r = fread(buffer, sizeof(uint8_t), OFFSET, patch);
        if (r != OFFSET) {
            finish(1, toClose, stderr, "failed to read patch offset\n");
        }

        size_t offset = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
        int readEof = strncmp((const char *) buffer, "EOF", OFFSET) == 0;

        r = fread(buffer, sizeof(uint8_t), SIZE, patch);
        if (r != SIZE) {
            if (readEof && feof(patch)) {
                break;
            }

            finish(1, toClose, stderr, "failed to read patch size\n");
        }

        fseek(patched, offset, SEEK_SET);
        size_t size = (buffer[0] << 8) | buffer[1];

        if (size == RLE) {
            r = fread(buffer, sizeof(uint8_t), RLE_SIZE, patch);
            if (r != RLE_SIZE) {
                finish(1, toClose, stderr, "failed to read patch RLE size\n");
            }

            size_t rleSize = (buffer[0] << 8) | buffer[1];

            r = fread(buffer, sizeof(uint8_t), RLE_VALUE, patch);
            if (r != RLE_VALUE) {
                finish(1, toClose, stderr, "failed to read patch RLE value\n");
            }

            uint8_t value = buffer[0];
            size_t s = rleSize > LENGTH ? LENGTH : rleSize;
            memset(buffer, value, s);

            size_t toWrite = rleSize;
            while (toWrite > 0) {
                size_t w = fwrite(buffer, sizeof(uint8_t), s, patched);

                if (w < 0) {
                    finish(1, toClose, stderr, "failed to write patch RLE data\n");
                }

                toWrite -= w;
            }
        } else {
            size_t toRead = size;

            while (toRead > 0) {
                size_t s = toRead > LENGTH ? LENGTH : toRead;
                r = fread(buffer, sizeof(uint8_t), s, patch);

                if (r < 0) {
                    finish(1, toClose, stderr, "failed to read patch data\n");
                }

                toRead -= r;
                size_t toWrite = r;
                size_t offset = 0;

                while (toWrite > 0) {
                    size_t w = fwrite(buffer + offset, sizeof(uint8_t), toWrite, patched);

                    if (w < 0) {
                        finish(1, toClose, stderr, "failed to write patch data\n");
                    }

                    toWrite -= w;
                    offset += w;
                }
            }
        }
    }

    finish(0, toClose, stdout, "patch applied successfully\n");
}