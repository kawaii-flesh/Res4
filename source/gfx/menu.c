#include "menu.h"

#include <mem/heap.h>
#include <mem/minerva.h>
#include <soc/timer.h>
#include <string.h>
#include <utils/btn.h>
#include <utils/sprintf.h>
#include <utils/util.h>

#include "../hid/hid.h"
#include "../utils/vector.h"
#include "gfx.h"
#include "gfxutils.h"

const char* sizeDefs[] = {"B ", "KB", "MB", "GB"};

void _printEntry(MenuEntry_t entry, u32 maxLen, u8 highlighted, u32 bg) {
    if (entry.hide) {
        return;
    }

    (highlighted) ? SETCOLOR(bg, RGBUnionToU32(entry.optionUnion)) : SETCOLOR(RGBUnionToU32(entry.optionUnion), bg);

    if (entry.icon) {
        gfx_putc(entry.icon);
        gfx_putc(' ');
        maxLen -= 2;
    }

    u32 curX = 0, curY = 0;
    gfx_con_getpos(&curX, &curY);
    gfx_puts_limit(entry.name, maxLen - ((entry.showSize) ? 8 : 0));
    if (entry.showSize) {
        (highlighted) ? SETCOLOR(bg, COLOR_BLUE) : SETCOLOR(COLOR_BLUE, bg);
        gfx_con_setpos(curX + (maxLen - 6) * 16, curY);
        gfx_printf("%4d", entry.size);
        gfx_puts_small(sizeDefs[entry.sizeDef]);
    }

    gfx_putc('\n');
}

int newMenu(Vector_t* vec, int startIndex, int screenLenX, int screenLenY, u8 options, int entryCount) {
    vecPDefArray(MenuEntry_t*, entries, vec);
    int selected = startIndex;

    while (entries[selected].skip || entries[selected].hide) {
        selected++;
        if (selected >= vec->count) {
            selected = 0;
        }
    }

    int lastIndex = selected;
    u32 startX = 0, startY = 0;
    gfx_con_getpos(&startX, &startY);

    u32 bgColor = (options & USELIGHTGREY) ? COLOR_DARKGREY : COLOR_DEFAULT;

    bool redrawScreen = true;
    Input_t* input = hidRead();

    // Maybe add a check here so you don't read OOB by providing a too high startindex?

    u32 lastPress = 500 + get_tmr_ms();
    u32 holdTimer = 300;
    int totalPageCount = ((vec->count - 1) / screenLenY) + 1;
    while (1) {
        int currentPage = (selected / screenLenY) + 1;
        u32 lastDraw = get_tmr_ms();
        if (redrawScreen || options & ALWAYSREDRAW) {
            if (options & ENABLEPAGECOUNT) {
                SETCOLOR(COLOR_DEFAULT, COLOR_WHITE);
                char temp[40] = "";
                s_printf(temp, " Page %d / %d | Total %d entries", currentPage, totalPageCount, entryCount);
                gfx_con_setpos(YLEFT - strlen(temp) * 18, 0);
                gfx_printf(temp);
            }

            gfx_con_setpos(startX, startY);

            if (redrawScreen) {
                gfx_boxGrey(startX, startY, startX + screenLenX * 16, startY + screenLenY * 16, (options & USELIGHTGREY) ? 0x33 : 0x1B);
            }

            int start = selected / screenLenY * screenLenY;
            gfx_con_setpos(startX, startY);
            gfx_printf("%b", startX);
            for (int i = start; i < MIN(vec->count, start + screenLenY); i++) {
                _printEntry(entries[i], screenLenX, (i == selected), bgColor);
            }
            gfx_printf("%b", 0);
        } else if (lastIndex != selected) {
            u32 minLastCur = MIN(lastIndex, selected);
            u32 maxLastCur = MAX(lastIndex, selected);
            gfx_con_setpos(startX, startY + ((minLastCur % screenLenY) * 16));
            _printEntry(entries[minLastCur], screenLenX, (minLastCur == selected), bgColor);
            gfx_con_setpos(startX, startY + ((maxLastCur % screenLenY) * 16));
            _printEntry(entries[maxLastCur], screenLenX, (minLastCur != selected), bgColor);
        }

        lastIndex = selected;

        SETCOLOR(COLOR_DEFAULT, COLOR_WHITE);
        gfx_con_setpos(0, 704);
        gfx_printf("Time taken for screen draw: %dms  ", get_tmr_ms() - lastDraw);

        while (hidRead()) {
            if (!(input->buttons)) {
                holdTimer = 300;
                break;
            }

            if (input->buttons & (JoyRUp | JoyRDown)) {
                holdTimer = 40;
            }

            if ((lastPress + holdTimer) < get_tmr_ms()) {
                if (holdTimer > 50) {
                    holdTimer -= 50;
                }
                break;
            }
        }

        int currentPageFirstIndex = (currentPage - 1) * screenLenY;
        int nextPageFirstIndex = currentPageFirstIndex + screenLenY;
        bool pageTurn = false;
        while (1) {
            if (hidRead()->a) {
                return selected;
            } else if (input->b && options & ENABLEB) {
                return -1;
            } else if (input->down || input->rDown) {  // Rdown should probs not trigger a page change. Same for RUp
                ++selected;
                break;
            } else if (input->right) {
                if (totalPageCount == 1) {
                    selected = vec->count - 1;
                    break;
                } else if (totalPageCount > 1) {
                    if (currentPage == totalPageCount) {
                        selected = 0;
                    } else {
                        selected = nextPageFirstIndex;
                    }
                    while (selected < vec->count && entries[selected].optionUnion & SKIPHIDEBITS) {
                        ++selected;
                    }
                    pageTurn = true;
                    break;
                }
            } else if (input->up || input->rUp) {
                --selected;
                break;
            } else if (input->left) {
                if (totalPageCount == 1) {
                    selected = currentPageFirstIndex;
                    while (selected < vec->count - 1 && entries[selected].optionUnion & SKIPHIDEBITS) {
                        ++selected;
                    }
                    break;
                } else if (totalPageCount > 1) {
                    if (currentPage == 1) {
                        selected = (totalPageCount - 1) * screenLenY;
                    } else {
                        selected = (currentPage - 2) * screenLenY;
                    }
                    while (selected < vec->count && entries[selected].optionUnion & SKIPHIDEBITS) {
                        ++selected;
                    }
                    pageTurn = true;
                    break;
                }
            } else {
                holdTimer = 300;
                gfx_printTopInfo();
            }
        }

        lastPress = get_tmr_ms();

        if ((selected < lastIndex && !pageTurn) || selected == vec->count - 1) {
            while (selected > currentPageFirstIndex - 1 && entries[selected].optionUnion & SKIPHIDEBITS) {
                --selected;
            }
            if (selected < currentPageFirstIndex) {
                selected = totalPageCount == 1 ? vec->count - 1 : MIN(nextPageFirstIndex - 1, vec->count - 1);
                while (selected != 0 && entries[selected].optionUnion & SKIPHIDEBITS) {
                    --selected;
                }
            }
        } else if (selected > lastIndex && !pageTurn) {
            while (selected < vec->count && entries[selected].optionUnion & SKIPHIDEBITS) {
                ++selected;
            }
            if (selected >= nextPageFirstIndex || selected >= vec->count) {
                selected = currentPageFirstIndex;
                while (selected < vec->count - 1 && entries[selected].optionUnion & SKIPHIDEBITS) {
                    ++selected;
                }
            }
        }

        redrawScreen = (selected / screenLenY != lastIndex / screenLenY);
    }
}

void freeAllocatedNames(MenuEntry_t mainMenuEntries[], u32 count) {
    for (int i = 0; i < count; ++i) {
        if (mainMenuEntries[i].optionUnion & ALLOCATED_NAME_BIT) {
            free(mainMenuEntries[i].name);
        }
    }
}
