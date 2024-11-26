#include "tar.h"

#include <string.h>
#include <utils/sprintf.h>

#include "../../microtar/src/microtar.h"
#include "print.h"
#include "rw.h"

const char* getLastPathFragment(const char* path) {
    const char* lastSlash = strrchr(path, '/');

    if (lastSlash && *(lastSlash + 1) != '\0') {
        return lastSlash + 1;
    } else {
        return path;
    }
}

int extractTar(const char* tarFilename, const char* destDir) {
    mtar_t tar;
    mtar_header_t header;
    char fullPath[256];
    FRESULT res;
    FIL file;
    unsigned long totalSize = 0;
    unsigned long extractedSize = 0;

    if (mtar_open(&tar, tarFilename, "r") != MTAR_ESUCCESS) {
        print(ERROR, "Failed to open TAR file: %s\n", tarFilename);
        return 0;
    }

    while (mtar_read_header(&tar, &header) != MTAR_ENULLRECORD) {
        if (header.type == MTAR_TREG) {
            totalSize += header.size;
        }
        mtar_next(&tar);
    }

    mtar_rewind(&tar);

    if (strcmp(destDir, "/") != 0 && createDirIfNotExist(destDir) != FR_OK) {
        print(ERROR, "Failed to create destination directory: %s\n", destDir);
        mtar_close(&tar);
        return 0;
    }

    const size_t bufferSize = 64 * 1024 * 1024;
    char* buffer = (char*)malloc(bufferSize);
    if (!buffer) {
        print(ERROR, "Memory allocation failed for buffer\n");
        mtar_close(&tar);
        return 0;
    }

    while (mtar_read_header(&tar, &header) != MTAR_ENULLRECORD) {
        if (strlen(destDir) + strlen(header.name) + 1 >= sizeof(fullPath)) {
            print(ERROR, "Path length exceeds buffer size: %s/%s\n", destDir, header.name);
            free(buffer);
            mtar_close(&tar);
            return 0;
        }

        s_printf(fullPath, "%s/%s", destDir, header.name);
        normalizePath(fullPath);

        if (header.type == MTAR_TDIR) {
            if (!createDirectories(fullPath)) {
                free(buffer);
                mtar_close(&tar);
                return 0;
            }
        } else if (header.type == MTAR_TREG) {
            char* lastSlash = strrchr(fullPath, '/');
            if (lastSlash) {
                *lastSlash = '\0';
                if (!createDirectories(fullPath)) {
                    free(buffer);
                    mtar_close(&tar);
                    return 0;
                }
                *lastSlash = '/';
            }

            res = f_open(&file, fullPath, FA_WRITE | FA_CREATE_ALWAYS);
            if (res != FR_OK) {
                print(ERROR, "Failed to open file for writing: %s\n", fullPath);
                free(buffer);
                mtar_close(&tar);
                return 0;
            }

            int remaining = header.size;
            while (remaining > 0) {
                int toRead = remaining > bufferSize ? bufferSize : remaining;
                mtar_read_data(&tar, buffer, toRead);
                unsigned int written;
                res = f_write(&file, buffer, toRead, &written);

                if (res != FR_OK || written != toRead) {
                    f_close(&file);
                    free(buffer);
                    mtar_close(&tar);
                    return 0;
                }

                remaining -= toRead;
                extractedSize += written;

                print(INFO, "\rextracted: %d of %d bytes ", extractedSize, totalSize);
            }
            f_close(&file);
        }

        mtar_next(&tar);
    }

    free(buffer);
    mtar_close(&tar);
    return 1;
}
