/* SPDX-License-Identifier: MPL-2.0 */
/* J6: the app's half of the contract with the platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "J6"
#define APP_SPLASH_RGB RGB(236, 90, 40)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */
#define APP_OVERLOAD                     /* the render ran late: shed a voice */
void app_overload(void);

/* the store: settings, the 56 memories, and the platform's panel calibration */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_PATCH, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDA000u}

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

/* host simulator checks (host/plat_host.c "expect"): patch, page, sub, dirty, octave, voices, arp, hold, cutoff, res,
 * mode, chorus, tune, note (the last key's MIDI note) */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
