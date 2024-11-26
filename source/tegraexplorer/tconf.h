#pragma once
#include <utils/types.h>

enum { LOC_None = 0, LOC_SD, LOC_EMMC, LOC_EMUMMC };

enum { CMODE_None = 0, CMODE_Copy, CMODE_Move, CMODE_CopyFolder, CMODE_MoveFolder };

typedef struct {
    u32 FSBuffSize;
    char* srcCopy;
    int isMariko;
    u32 errors;
    union {
        struct {
            u16 minervaEnabled : 1;
            u16 curExplorerLoc : 2;
            u16 heldExplorerCopyLoc : 2;
            u16 explorerCopyMode : 4;
        };
        u16 optionUnion;
    };
} TConf_t;

extern TConf_t TConf;

void ResetCopyParams();
void SetCopyParams(const char* path, u8 mode);
