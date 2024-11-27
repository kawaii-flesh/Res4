/*
 * Copyright (c) 2018 naehrwert
 *
 * Copyright (c) 2018-2020 CTCaer
 * Copyright (c) 2019-2020 shchmue
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <display/di.h>
#include <gfx_utils.h>
#include <input/joycon.h>
#include <libs/fatfs/ff.h>
#include <mem/heap.h>
#include <mem/minerva.h>
#include <power/bq24193.h>
#include <power/max17050.h>
#include <power/max77620.h>
#include <rtc/max77620-rtc.h>
#include <soc/bpmp.h>
#include <soc/hw_init.h>
#include <soc/pmc.h>
#include <soc/t210.h>
#include <soc/timer.h>
#include <storage/sd.h>
#include <storage/sdmmc.h>
#include <string.h>
#include <utils/btn.h>
#include <utils/dirlist.h>
#include <utils/ini.h>
#include <utils/sprintf.h>
#include <utils/util.h>

#include "err.h"
#include "gfx/gfx.h"
#include "gfx/gfxutils.h"
#include "hid/hid.h"
#include "hid/touchutils.h"
#include "res4/gfx/gfx.h"
#include "res4/res4.h"
#include "tegraexplorer/tconf.h"

boot_cfg_t __attribute__((section("._boot_cfg"))) b_cfg;

volatile nyx_storage_t* nyx_str = (nyx_storage_t*)NYX_STORAGE_ADDR;

// This is a safe and unused DRAM region for our payloads.
#define RELOC_META_OFF 0x7C
#define PATCHED_RELOC_SZ 0x94
#define PATCHED_RELOC_STACK 0x40007000
#define PATCHED_RELOC_ENTRY 0x40010000
#define EXT_PAYLOAD_ADDR 0xC0000000
#define RCM_PAYLOAD_ADDR (EXT_PAYLOAD_ADDR + ALIGN(PATCHED_RELOC_SZ, 0x10))
#define COREBOOT_END_ADDR 0xD0000000
#define CBFS_DRAM_EN_ADDR 0x4003e000
#define CBFS_DRAM_MAGIC 0x4452414D  // "DRAM"

static void* coreboot_addr;

void reloc_patcher(u32 payload_dst, u32 payload_src, u32 payload_size) {
    memcpy((u8*)payload_src, (u8*)IPL_LOAD_ADDR, PATCHED_RELOC_SZ);

    volatile reloc_meta_t* relocator = (reloc_meta_t*)(payload_src + RELOC_META_OFF);

    relocator->start = payload_dst - ALIGN(PATCHED_RELOC_SZ, 0x10);
    relocator->stack = PATCHED_RELOC_STACK;
    relocator->end = payload_dst + payload_size;
    relocator->ep = payload_dst;

    if (payload_size == 0x7000) {
        memcpy((u8*)(payload_src + ALIGN(PATCHED_RELOC_SZ, 0x10)), coreboot_addr, 0x7000);  // Bootblock
        *(vu32*)CBFS_DRAM_EN_ADDR = CBFS_DRAM_MAGIC;
    }
}

int launch_payload(char* path) {
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
    if (!path) {
        return 1;
    }

    if (sd_mount()) {
        FIL fp;
        if (f_open(&fp, path, FA_READ)) {
            EPRINTFARGS("Payload file is missing!\n(%s)", path);
            sd_unmount();

            return 1;
        }

        // Read and copy the payload to our chosen address
        void* buf;
        u32 size = f_size(&fp);

        if (size < 0x30000) {
            buf = (void*)RCM_PAYLOAD_ADDR;
        } else {
            coreboot_addr = (void*)(COREBOOT_END_ADDR - size);
            buf = coreboot_addr;
        }

        if (f_read(&fp, buf, size, NULL)) {
            f_close(&fp);
            sd_unmount();

            return 1;
        }

        f_close(&fp);

        sd_unmount();

        if (size < 0x30000) {
            reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, ALIGN(size, 0x10));

            hw_deinit(false, byte_swap_32(*(u32*)(buf + size - sizeof(u32))));
        } else {
            reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, 0x7000);
            hw_deinit(true, 0);
        }

        // Some cards (Sandisk U1), do not like a fast power cycle. Wait min 100ms.
        sdmmc_storage_init_wait_sd();

        void (*ext_payload_ptr)() = (void*)EXT_PAYLOAD_ADDR;

        // Launch our payload.
        (*ext_payload_ptr)();
    }

    return 1;
}

extern void pivot_stack(u32 stack_top);

#define EXCP_EN_ADDR 0x4003FFFC
#define EXCP_MAGIC 0x30505645  // EVP0
#define EXCP_TYPE_ADDR 0x4003FFF8
#define EXCP_TYPE_RESET 0x545352    // RST
#define EXCP_TYPE_UNDEF 0x464455    // UDF
#define EXCP_TYPE_PABRT 0x54424150  // PABT
#define EXCP_TYPE_DABRT 0x54424144  // DABT
#define EXCP_LR_ADDR 0x4003FFF4

static inline void _show_errors() {
    u32* excp_enabled = (u32*)EXCP_EN_ADDR;
    u32* excp_type = (u32*)EXCP_TYPE_ADDR;
    u32* excp_lr = (u32*)EXCP_LR_ADDR;

    if (*excp_enabled == EXCP_MAGIC) {
        TConf.errors |= ERR_EXCEPTION;
    }

    if (TConf.errors) {
        /*
        if (h_cfg.errors & ERR_SD_BOOT_EN)
                WPRINTF("Failed to mount SD!\n");

        if (h_cfg.errors & ERR_LIBSYS_LP0)
                WPRINTF("Missing LP0 (sleep mode) lib!\n");
        if (h_cfg.errors & ERR_LIBSYS_MTC)
                WPRINTF("Missing or old Minerva lib!\n");

        if (h_cfg.errors & (ERR_LIBSYS_LP0 | ERR_LIBSYS_MTC))
                WPRINTF("\nUpdate bootloader folder!\n\n");
        */

        if (TConf.errors & ERR_EXCEPTION) {
            gfx_clearscreen();
            WPRINTFARGS("LR %08X", *excp_lr);
            u32 exception = 0;

            switch (*excp_type) {
                case EXCP_TYPE_RESET:
                    exception = TE_EXCEPTION_RESET;
                    break;
                case EXCP_TYPE_UNDEF:
                    exception = TE_EXCEPTION_UNDEFINED;
                    break;
                case EXCP_TYPE_PABRT:
                    exception = TE_EXCEPTION_PREF_ABORT;
                    break;
                case EXCP_TYPE_DABRT:
                    exception = TE_EXCEPTION_DATA_ABORT;
                    break;
            }

            // Clear the exception.
            *excp_enabled = 0;
            DrawError(newErrCode(exception));
        }
    }
}

void ipl_main() {
    // Do initial HW configuration. This is compatible with consecutive reruns without a reset.
    hw_init();
    jc_init_hw();

    // Pivot the stack so we have enough space.
    pivot_stack(IPL_STACK_TOP);

    // Tegra/Horizon configuration goes to 0x80000000+, package2 goes to 0xA9800000, we place our heap in between.
    heap_init((void*)IPL_HEAP_START);

    // Mount SD Card.
    TConf.errors |= !sd_mount() ? ERR_SD_BOOT_EN : 0;

    // Enable watchdog protection to avoid SD corruption based hanging in LP0/Minerva config.
    watchdog_start(5000000 / 2, TIMER_FIQENABL_EN);  // 5 seconds.
    TConf.minervaEnabled = !minerva_init();
    // Disable watchdog protection.
    watchdog_end();
    TConf.FSBuffSize = (TConf.minervaEnabled) ? 0x800000 : 0x10000;
    TConf.isMariko = hw_get_chip_id() == GP_HIDREV_MAJOR_T210B01;

    if (!TConf.minervaEnabled) {  //! TODO: Add Tegra210B01 support to minerva.
        TConf.errors |= ERR_LIBSYS_MTC;
    }

    // Prep RTC regs for read. Needed for T210B01 R2P.
    max77620_rtc_prep_read();

    // Initialize display.
    display_init();

    u32* fb = display_init_window_a_pitch();
    gfx_init_ctxt(fb, 720, 1280, 720);

    gfx_con_init();

    display_backlight_pwm_init();
    display_backlight_brightness(100, 1000);

    if (hidRead()->volm) {
        // Initialize touch.
        *isTouchEnabled() = touch_power_on();
    }

    // Overclock BPMP.
    bpmp_clk_rate_set(TConf.isMariko ? BPMP_CLK_DEFAULT_BOOST : BPMP_CLK_LOWER_BOOST);

    _show_errors();

    // Set ram to a freq that doesn't need periodic training.
    minerva_change_freq(FREQ_800);
    gfx_clearscreenR4();
    gfx_printf("Waiting for the JoyCons to be ready. Or press power button to continue ...");
    while (!hidConnected() && !*isTouchEnabled() && !hidRead()->power) {
    }

    Res4();

    // Halt BPMP if we managed to get out of execution.
    while (true) {
        bpmp_halt();
    }
}
