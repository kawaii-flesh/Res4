#include "rw.h"

#include <mem/heap.h>
#include <string.h>
#include <utils/sprintf.h>

#include "print.h"

int createDirIfNotExist(const char* path) {
    FRESULT res = f_stat(path, NULL);
    if (res == FR_NO_FILE) {
        res = f_mkdir(path);
    }
    return (res == FR_OK || res == FR_EXIST);
}

int normalizePath(char* path) {
    char *src = path, *dst = path;
    while (*src) {
        *dst = *src++;
        if (*dst != '/' || (dst > path && *(dst - 1) != '/')) {
            dst++;
        }
    }
    *dst = '\0';
    return 1;
}

int createDirectories(const char* path) {
    if (strcmp(path, "/") == 0) {
        print(ERROR, "Path is root directory, no need to create directories.\n");
        return 1;
    }

    char temp_path[256];
    if (strlen(path) >= sizeof(temp_path)) {
        print(ERROR, "Path length exceeds buffer size.\n");
        return 0;
    }
    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';

    char* p = temp_path;
    while (*p) {
        if (*p == '/') {
            p++;
            continue;
        }

        while (*p && *p != '/') {
            p++;
        }

        char old = *p;
        *p = '\0';

        if (!createDirIfNotExist(temp_path)) {
            print(ERROR, "Failed to create directory: %s\n", temp_path);
            return 0;
        }

        *p = old;
        if (*p) {
            p++;
        }
    }
    return 1;
}

#define COPY_BUFFER_SIZE (10 * 1024 * 1024)
int copyFile(const char* srcPath, const char* destPath) {
    FIL srcFile, destFile;
    char* buffer = (char*)malloc(COPY_BUFFER_SIZE);
    unsigned int bytesRead, bytesWritten;

    if (!buffer) {
        print(ERROR, "Failed to allocate memory for buffer.\n");
        return 0;
    }

    char destDirPath[256];
    strncpy(destDirPath, destPath, sizeof(destDirPath) - 1);
    destDirPath[sizeof(destDirPath) - 1] = '\0';
    char* lastSlash = strrchr(destDirPath, '/');
    if (lastSlash) {
        *lastSlash = '\0';
        if (!createDirectories(destDirPath)) {
            free(buffer);
            return 0;
        }
    }

    FRESULT res = f_open(&srcFile, srcPath, FA_READ);
    if (res != FR_OK) {
        print(ERROR, "Failed to open source file: %s\n", srcPath);
        free(buffer);
        return 0;
    }

    res = f_open(&destFile, destPath, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        print(ERROR, "Failed to open destination file: %s\n", destPath);
        f_close(&srcFile);
        free(buffer);
        return 0;
    }

    while (1) {
        res = f_read(&srcFile, buffer, COPY_BUFFER_SIZE, &bytesRead);
        if (res != FR_OK || bytesRead == 0) {
            if (res != FR_OK) {
                print(ERROR, "Error reading from source file: %s\n", srcPath);
            }
            break;
        }

        res = f_write(&destFile, buffer, bytesRead, &bytesWritten);
        if (res != FR_OK || bytesWritten < bytesRead) {
            print(ERROR, "Error writing to destination file: %s\n", destPath);
            f_close(&srcFile);
            f_close(&destFile);
            rm(destPath);
            free(buffer);
            return 0;
        }
    }

    f_close(&srcFile);
    f_close(&destFile);
    free(buffer);
    return 1;
}

int copyDirectory(const char* pathFrom, const char* pathTo) {
    DIR srcDir;
    FILINFO fno;
    char srcPath[256], destPath[256];

    if (f_opendir(&srcDir, pathFrom) != FR_OK) {
        print(ERROR, "Failed to open source directory: %s\n", pathFrom);
        return 0;
    }

    if (!createDirIfNotExist(pathTo)) {
        f_closedir(&srcDir);
        return 0;
    }

    while (f_readdir(&srcDir, &fno) == FR_OK) {
        if (fno.fname[0] == 0) {
            break;
        }

        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0) {
            continue;
        }

        if (strlen(pathFrom) + strlen(fno.fname) + 1 >= sizeof(srcPath) || strlen(pathTo) + strlen(fno.fname) + 1 >= sizeof(destPath)) {
            print(ERROR, "Path length exceeds buffer size: %s/%s\n", pathFrom, fno.fname);
            f_closedir(&srcDir);
            return 0;
        }

        s_printf(srcPath, "%s/%s", pathFrom, fno.fname);
        s_printf(destPath, "%s/%s", pathTo, fno.fname);

        if (fno.fattrib & AM_DIR) {
            if (!copyDirectory(srcPath, destPath)) {
                print(ERROR, "Failed to copy directory: %s\n", srcPath);
                f_closedir(&srcDir);
                return 0;
            }
        } else {
            if (!copyFile(srcPath, destPath)) {
                f_closedir(&srcDir);
                return 0;
            }
        }
    }
    f_closedir(&srcDir);
    return 1;
}

int copy(const char* pathFrom, const char* pathTo) {
    FILINFO fno;
    if (f_stat(pathFrom, &fno) != FR_OK) {
        return 0;
    }

    if (fno.fattrib & AM_DIR) {
        return copyDirectory(pathFrom, pathTo);
    } else {
        return copyFile(pathFrom, pathTo);
    }
}

int rmRecursive(const char* path) {
    DIR dir;
    FILINFO fno;
    FRESULT res = f_opendir(&dir, path);

    if (res != FR_OK) {
        print(ERROR, "Failed to open directory: %s\n", path);
        return 0;
    }

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) {
            break;
        }

        if (fno.fattrib & AM_DIR) {
            if (strcmp(fno.fname, ".") != 0 && strcmp(fno.fname, "..") != 0) {
                char subdir[256];
                if (strlen(path) + strlen(fno.fname) + 1 >= sizeof(subdir)) {
                    print(ERROR, "Path too long for subdirectory: %s/%s\n", path, fno.fname);
                    f_closedir(&dir);
                    return 0;
                }
                s_printf(subdir, "%s/%s", path, fno.fname);
                if (!rmRecursive(subdir)) {
                    f_closedir(&dir);
                    return 0;
                }
            }
        } else {
            char file_path[256];
            if (strlen(path) + strlen(fno.fname) + 1 >= sizeof(file_path)) {
                print(ERROR, "Path too long for file: %s/%s\n", path, fno.fname);
                f_closedir(&dir);
                return 0;
            }
            s_printf(file_path, "%s/%s", path, fno.fname);
            if (f_unlink(file_path) != FR_OK) {
                print(ERROR, "Failed to remove file: %s\n", file_path);
                f_closedir(&dir);
                return 0;
            }
        }
    }

    f_closedir(&dir);
    if (f_rmdir(path) != FR_OK) {
        print(ERROR, "Failed to remove directory: %s\n", path);
        return 0;
    }
    return 1;
}

int rm(const char* filename) {
    FILINFO fno;
    FRESULT res = f_stat(filename, &fno);

    if (res != FR_OK) {
        return 0;
    }

    if (fno.fattrib & AM_DIR) {
        if (!rmRecursive(filename)) {
            return 0;
        }
    } else {
        if (f_unlink(filename) != FR_OK) {
            print(ERROR, "Failed to remove file: %s\n", filename);
            return 0;
        }
    }
    return 1;
}
