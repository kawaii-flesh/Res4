#include "pscript.h"

#include <libs/fatfs/ff.h>
#include <string.h>
#include <utils/sprintf.h>

#include "gfx/dialogs/confirmationDialog.h"
#include "gfx/gfx.h"
#include "helpers/print.h"
#include "helpers/rw.h"
#include "helpers/tar.h"

int extract(const char* archive, const char* outdir) { return extractTar(archive, outdir); }

int executeScript(const char* filename) {
    FIL file;
    FRESULT res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        print(ERROR, "Failed to open script file: %s\n", filename);
        return 0;
    }

    char line[256];
    while (f_gets(line, sizeof(line), &file)) {
        line[strcspn(line, "\n")] = '\0';
        char originalLine[256];
        strcpy(originalLine, line);
        char* command = strtok(line, "|");
        char* originalCommand = command;
        char* param1 = strtok(NULL, "|");
        char* param2 = strtok(NULL, "|");

        if (command) {
            int res = 0;
            int forceSuccess = (command[0] == '~');
            if (forceSuccess) {
                command++;
            } else if (command[0] == '?') {
                command++;
                const char* executionConfirm[] = {"Do you want to execute this command?", command, param1, param2, NULL};
                const enum ConfirmationDialogResult cresult = confirmationDialog(executionConfirm, ENO);
                gfx_clearscreenR4();
                if (cresult == ENO) {
                    continue;
                }
            }

            print(INFO, "%s ", originalLine);
            if (strcmp(command, "rm") == 0) {
                if (!param1) {
                    print(ERROR, "Missing parameter for command 'rm'. Expected: rm|<file/directory>\n");
                } else {
                    res = rm(param1);
                }
            } else if (strcmp(command, "cp") == 0) {
                if (!param1 || !param2) {
                    print(ERROR, "Missing parameter(s) for command 'cp'. Expected: cp|<source>|<destination>\n");
                } else {
                    res = copy(param1, param2);
                }
            } else if (strcmp(command, "extract") == 0) {
                if (!param1 || !param2) {
                    print(ERROR, "Missing parameter(s) for command 'extract'. Expected: extract|<tar archive>|<destination>\n");
                } else {
                    print(INFO, "\n");
                    res = extract(param1, param2);
                }
            } else {
                print(ERROR, "Unknown command\n");
                res = 0;
            }

            if (!res && forceSuccess) {
                print(WARNING, "Force ok\n");
            } else if (res) {
                print(GOOD, "Ok\n");
            } else {
                print(ERROR, "\nCommand execution failed:\n%s\n", originalLine);
                const char* commandErrorMsg[] = {"An error occurred while executing the command.", "Do you want to continue anyway?", NULL};
                if (confirmationDialog(commandErrorMsg, ENO) == ENO) {
                    gfx_clearscreenR4();
                    f_close(&file);
                    return 0;
                }
                gfx_clearscreenR4();
            }
        } else {
            print(ERROR, "Failed to parse command in script line: '%s'\n", line);
        }
    }

    f_close(&file);
    return 1;
}
