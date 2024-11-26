#pragma once
#define MAX_SHA256_LEN 64

typedef struct {
    char sha256Hex[MAX_SHA256_LEN + 1];
    char* filePath;
    unsigned int offset;
} SHA256Entry;

// 1 - good | 0 - bad
int calculateSHA256(const char* filePath, unsigned int offset, char* hexStringOut);
int parseSHA256File(const char* filePath, SHA256Entry** entries, int* count);
void freeSHA256Entries(SHA256Entry* entries, int count);
int compareSHA256Strings(const char* str1, const char* str2);
