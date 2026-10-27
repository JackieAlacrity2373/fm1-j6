/* SPDX-License-Identifier: MPL-2.0 */
#include "j6_vibe.h"
#include "../src/juno.h"

#define BLOCK 256u

void j6v_init(void) { j_init(); }

void j6v_midi(uint8_t status, uint8_t d1, uint8_t d2)
{
    if ((status & 0xF0u) == 0xC0u) j6v_program(d1);
    else j_event(status, d1, d2);
}

void j6v_program(int n)
{
    int c = j_factory_count();
    if (c > 0) j_factory(n < 0 ? 0 : n % c, &j_p);
}

void j6v_process(float *left, float *right, uint32_t n)
{
    static int32_t buf[BLOCK * 2u];
    while (n) {
        uint32_t m = n < BLOCK ? n : BLOCK, i;
        j_render(buf, m, 8300000.0f);
        for (i = 0; i < m; i++) {
            left[i] = (float)buf[2u * i] * (1.0f / 8388608.0f);
            right[i] = (float)buf[2u * i + 1u] * (1.0f / 8388608.0f);
        }
        left += m; right += m; n -= m;
    }
}
