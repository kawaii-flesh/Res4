#include "res4.h"

#include <mem/heap.h>
#include <soc/fuse.h>
#include <storage/sd.h>
#include <utils/btn.h>
#include <utils/util.h>

#include "../utils/utils.h"
#include "gfx/dialogs/confirmationDialog.h"
#include "gfx/gfx.h"
#include "helpers/print.h"
#include "helpers/sha256.h"
#include "pscript.h"

#define RES4_HOME "/res4"
#define SHA256_LIST_FILE RES4_HOME "/sha256.list"
#define PSCRIPT_FILE RES4_HOME "/post.script"
#define POST_SHA256_LIST_FILE RES4_HOME "/sha256_post.list"

void Res4() {
    gfx_clearscreenR4();
    const char* confirmMessages[] = {"Do you want to run Res4?", NULL};
    const enum ConfirmationDialogResult confirmResult = confirmationDialog(confirmMessages, ENO);
    gfx_clearscreenR4();
    int someError = 0;
    if (confirmResult == EYES) {
        char sha256Hex[65];
        SHA256Entry* sha256List;
        int entriesCount;
        if (sd_mount()) {
            print(INFO, "The SD card has been mounted\n");
            const int parseResult = parseSHA256File(SHA256_LIST_FILE, &sha256List, &entriesCount);
            if (!parseResult) {
                someError = 1;
                print(ERROR, "An error occurred while parsing the verification list\n");
            } else {
                print(INFO, "\nThe verification list has been parsed\n");
                print(INFO, "Start checking checksums\n");
                int compareResult = 0;
                for (int i = 0; i < entriesCount; ++i) {
                    const char* filePath = sha256List[i].filePath;
                    print(INFO, "target: %s offset: %d\n", filePath, sha256List[i].offset);
                    print(GOOD, "expected: %s\n", sha256List[i].sha256Hex);
                    int calcResult = calculateSHA256(filePath, sha256List[i].offset, sha256Hex);
                    if (!calcResult) {
                        print(ERROR, "An error occurred while calculating sha256 for: %s\n", filePath);
                        someError = 1;
                        compareResult = 0;
                        break;
                    }
                    compareResult = compareSHA256Strings(sha256Hex, sha256List[i].sha256Hex);
                    Res4Colors compareResultColor = compareResult ? GOOD : ERROR;
                    print(compareResultColor, "actual  : %s\n", sha256Hex);
                    if (!compareResult) {
                        print(ERROR, "The checksums are different\n");
                        const char* checksumsDifMsg[] = {"The checksums are different", "Do you want to continue anyway?", NULL};
                        if (confirmationDialog(checksumsDifMsg, ENO) == ENO) {
                            someError = 1;
                            gfx_clearscreenR4();
                            break;
                        }
                        compareResult = 1;
                        gfx_clearscreenR4();
                    }
                }
                freeSHA256Entries(sha256List, entriesCount);
                if (compareResult) {
                    print(INFO, "\nThe execution of the post script has started. Wait for completion\n");
                    int res = executeScript(PSCRIPT_FILE);
                    if (!res) {
                        someError = 1;
                        print(ERROR, "An error occurred during the execution of the post script\n");
                    } else {
                        FILINFO fno;
                        int postVerificationResult = 1;
                        if (f_stat(POST_SHA256_LIST_FILE, &fno) == FR_OK) {
                            const int parseResult = parseSHA256File(POST_SHA256_LIST_FILE, &sha256List, &entriesCount);
                            if (!parseResult) {
                                someError = 1;
                                print(ERROR, "An error occurred while parsing the post verification list\n");
                            } else {
                                print(INFO, "\nThe post verification list has been parsed\n");
                                print(INFO, "Start checking checksums\n");
                                compareResult = 0;
                                for (int i = 0; i < entriesCount; ++i) {
                                    const char* filePath = sha256List[i].filePath;
                                    print(INFO, "target: %s offset: %d\n", filePath, sha256List[i].offset);
                                    print(GOOD, "expected: %s\n", sha256List[i].sha256Hex);
                                    int calcResult = calculateSHA256(filePath, sha256List[i].offset, sha256Hex);
                                    if (!calcResult) {
                                        print(ERROR, "An error occurred while calculating sha256 for: %s\n", filePath);
                                        someError = 1;
                                        compareResult = 0;
                                        break;
                                    }
                                    compareResult = compareSHA256Strings(sha256Hex, sha256List[i].sha256Hex);
                                    Res4Colors compareResultColor = compareResult ? GOOD : ERROR;
                                    print(compareResultColor, "actual  : %s\n", sha256Hex);
                                    if (!compareResult) {
                                        print(ERROR, "The checksums are different\n");
                                        const char* checksumsDifMsg[] = {"The checksums are different", "Do you want to continue anyway?", NULL};
                                        if (confirmationDialog(checksumsDifMsg, ENO) == ENO) {
                                            someError = 1;
                                            gfx_clearscreenR4();
                                            break;
                                        }
                                        compareResult = 1;
                                        gfx_clearscreenR4();
                                    }
                                }
                                freeSHA256Entries(sha256List, entriesCount);
                                postVerificationResult = compareResult;
                            }
                        }
                        if (postVerificationResult) {
                            print(GOOD, "\nDone!\n");
                        }
                    }
                }
            }
        } else {
            someError = 1;
            print(ERROR, "Failed to mount the SD card\n");
        }
        if (someError) {
            print(ERROR, "\nError!\n");
        }
        print(COLOR_ORANGE, "\nPress the power button to turn off\n");
        btn_wait();
    }
    power_off();
}
