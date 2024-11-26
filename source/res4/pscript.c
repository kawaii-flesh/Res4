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

        char* command = strtok(line, "|");
        char* param1 = strtok(NULL, "|");
        char* param2 = strtok(NULL, "|");

        if (command) {
            int res = 0;
            int forceSuccess = (command[0] == '~');
            if (forceSuccess) {
                command++;
            }

            if (param2) {
                print(INFO, "%s: %s %s ", command, param1, param2);
            } else if (param1) {
                print(INFO, "%s: %s ", command, param1);
            } else {
                print(INFO, "%s ", command);
            }

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
                    res = extract(param1, param2);
                }
            } else {
                print(ERROR, "Unknown command: '%s'\n", command);
                res = 0;
            }

            if (!res && forceSuccess) {
                print(WARNING, "Force ok\n");
            } else if (res) {
                print(GOOD, "Ok\n");
            } else {
                print(ERROR, "Command execution failed: %s\n", command);
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
