#include "print.h"

#include <stdarg.h>

#include "../../gfx/gfx.h"
#include "../../gfx/gfxutils.h"
#include "../gfx/gfx.h"

void print(const Res4Colors color, const char* fmt, ...) {
    SETCOLOR(color, COLOR_DEFAULT);

    u32 oldFgColor = gfx_con.fgcol;
    u32 oldBgColor = gfx_con.bgcol;

    if (gfx_con.mute) {
        return;
    }

    u32 x, y;
    gfx_con_getpos(&x, &y);

    if (y >= 720 - 34) {
        gfx_clearscreenR4();
    }

    gfx_con.fgcol = oldFgColor;
    gfx_con.bgcol = oldBgColor;

    va_list ap;
    va_start(ap, fmt);
    gfx_vprintf(fmt, ap);
    va_end(ap);
}
