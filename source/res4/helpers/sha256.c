#include "sha256.h"

#include <ctype.h>
#include <mem/heap.h>
#include <sec/se.h>
#include <string.h>
#include <utils/sprintf.h>

#include "print.h"
#include "rw.h"

#define CHUNK_SIZE 10 * 1024 * 1024
#define SHA256_SIZE 32
#define INITIAL_CAPACITY 3

int compareSHA256Strings(const char* str1, const char* str2) {
    if (strlen(str1) != strlen(str2)) {
        return 0;
    }

    while (*str1 && *str2) {
        if (tolower((unsigned char)*str1) != tolower((unsigned char)*str2)) {
            return 0;
        }
        str1++;
        str2++;
    }

    return 1;
}

void sha256ToHexString(const u8* sha_buf, char* hex_str) {
    for (int i = 0; i < 32; i++) {
        s_printf(&hex_str[i * 2], "%02x", sha_buf[i]);
    }
    hex_str[64] = '\0';
}

int calculateSHA256(const char* filePath, unsigned int offset, char* hexStringOut) {
    FIL fil;
    FRESULT res = f_open(&fil, filePath, FA_READ);
    if (res != FR_OK) {
        return 0;
    }

    u8* chunkBuffer = malloc(CHUNK_SIZE);
    if (!chunkBuffer) {
        return 0;
    }

    u32 bytesRead;
    f_lseek(&fil, offset);
    res = f_read(&fil, chunkBuffer, CHUNK_SIZE, &bytesRead);
    if (res != FR_OK) {
        print(ERROR, "Failed to read from file: %s (Offset: %d)\n", filePath, offset);
        free(chunkBuffer);
        f_close(&fil);
        return 0;
    }
    u8 shaBuf[SHA256_SIZE];
    int calcRes = se_calc_sha256_oneshot(shaBuf, chunkBuffer, bytesRead);
    free(chunkBuffer);
    f_close(&fil);
    if (!calcRes) {
        print(ERROR, "SHA256 calculation failed for file: %s (Offset: %d)\n", filePath, offset);
        return 0;
    }
    sha256ToHexString(shaBuf, hexStringOut);
    return 1;
}

int expandEntriesArray(SHA256Entry** entries, int* capacity) {
    int new_capacity = (*capacity) * 2;
    SHA256Entry* new_entries = (SHA256Entry*)malloc(new_capacity * sizeof(SHA256Entry));
    if (!new_entries) {
        return 0;
    }

    for (int i = 0; i < *capacity; i++) {
        new_entries[i] = (*entries)[i];
    }

    free(*entries);
    *entries = new_entries;
    *capacity = new_capacity;
    return 1;
}

unsigned int parseUnsignedInt(const char* str) {
    unsigned int result = 0;
    while (*str) {
        if (*str < '0' || *str > '9') {
            return 0;
        }
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

int parseSHA256File(const char* filePath, SHA256Entry** entries, int* count) {
    FIL file;
    FRESULT res;
    res = f_open(&file, filePath, FA_READ);
    if (res != FR_OK) {
        print(ERROR, "Failed to open file: %s\n", filePath);
        return 0;
    }

    char line[256];
    int capacity = INITIAL_CAPACITY;
    *entries = (SHA256Entry*)malloc(capacity * sizeof(SHA256Entry));
    if (!*entries) {
        print(ERROR, "Memory allocation failed for entries\n");
        f_close(&file);
        return 0;
    }

    *count = 0;

    while (f_gets(line, sizeof(line), &file)) {
        line[strcspn(line, "\n")] = 0;

        char* delimiterPos1 = strchr(line, '|');
        if (!delimiterPos1) {
            print(ERROR, "Invalid line format (missing delimiter '|'): %s\n", line);
            return 0;
        }
        *delimiterPos1 = '\0';

        char* sha256Hex = line;
        if (strlen(sha256Hex) != MAX_SHA256_LEN) {
            print(ERROR, "Invalid SHA256 hash length: %s\n", sha256Hex);
            return 0;
        }

        char* delimiterPos2 = strchr(delimiterPos1 + 1, '|');
        if (!delimiterPos2) {
            print(ERROR, "Invalid line format (missing second delimiter '|'): %s\n", line);
            return 0;
        }
        *delimiterPos2 = '\0';

        unsigned int offset = parseUnsignedInt(delimiterPos1 + 1);

        char* filePath = delimiterPos2 + 1;

        if (*count >= capacity) {
            if (!expandEntriesArray(entries, &capacity)) {
                print(ERROR, "Failed to expand entries array\n");
                f_close(&file);
                return 0;
            }
        }

        strncpy((*entries)[*count].sha256Hex, sha256Hex, MAX_SHA256_LEN + 1);
        (*entries)[*count].offset = offset;

        (*entries)[*count].filePath = (char*)malloc(strlen(filePath) + 1);
        if (!(*entries)[*count].filePath) {
            print(ERROR, "Memory allocation failed for file path: %s\n", filePath);
            f_close(&file);
            return 0;
        }
        strcpy((*entries)[*count].filePath, filePath);

        (*count)++;
    }

    f_close(&file);
    return 1;
}

void freeSHA256Entries(SHA256Entry* entries, int count) {
    for (int i = 0; i < count; i++) {
        free(entries[i].filePath);
    }
    free(entries);
}
