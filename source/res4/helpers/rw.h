#pragma once
#include <libs/fatfs/ff.h>

int createDirIfNotExist(const char path[]);
int copy(const char* pathFrom, const char* pathTo);
int createDirectories(const char* path);
int normalizePath(char* path);
int rm(const char* filename);
