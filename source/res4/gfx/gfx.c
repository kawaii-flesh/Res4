#include "gfx.h"

#include <stdlib.h>
#include <string.h>

#include "../../gfx/gfx.h"
#include "../../gfx/gfxutils.h"
#include "../../hid/hid.h"
#include "../helpers/hw.h"

void gfx_printTopInfoR4() {
    SETCOLOR(COLOR_DEFAULT, COLOR_WHITE);
    gfx_con_setpos(0, 0);
    gfx_printf("Res4 %d.%d.%d | SoC: %s | Battery: %d%% %c\n", R4_VER_MJ, R4_VER_MN, R4_VER_BF,
               getHWType() == COMMON   ? "Common"
               : getHWType() == MARIKO ? "Mariko"
                                       : "Erista",
               getBatteryValue(), (getCurrentChargeState() ? 129 : 32));
    RESETCOLOR;
}

void gfx_printBottomInfoKT(const char* message) {
    gfx_boxGrey(0, 703, 1279, 719, 0xFF);
    gfx_con_setpos(0, 704);
    SETCOLOR(COLOR_DEFAULT, COLOR_WHITE);
    gfx_puts_limit(message, (1280 / gfx_con.fntsz) - 8);
}

void gfx_clearscreenR4() {
    gfx_boxGrey(0, 16, 1279, 703, 0x1b);

    gfx_boxGrey(0, 703, 1279, 719, 0xFF);
    gfx_boxGrey(0, 0, 1279, 15, 0xFF);

    gfx_printTopInfoR4();
}
