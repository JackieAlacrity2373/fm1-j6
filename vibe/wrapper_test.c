/* SPDX-License-Identifier: MPL-2.0 */
/* Host check of the VIBE wrapper:
 *   gcc -O2 -ffp-contract=off -I fm1-base/include -o build/vibe_test vibe/wrapper_test.c vibe/j6_vibe.c src/juno.c -lm */
#include <stdio.h>
#include <math.h>
#include "j6_vibe.h"

int main(void)
{
    static float l[1000], r[1000];
    float peak = 0.0f, tail = 0.0f;
    int i, k;
    j6v_init();
    j6v_midi(0xC0, 3, 0);
    j6v_midi(0x90, 60, 100);
    for (i = 0; i < 20; i++) {
        j6v_process(l, r, 1000);
        for (k = 0; k < 1000; k++) if (fabsf(l[k]) > peak) peak = fabsf(l[k]);
    }
    j6v_midi(0x80, 60, 0);
    for (i = 0; i < 100; i++) j6v_process(l, r, 1000);
    for (k = 0; k < 1000; k++) if (fabsf(l[k]) > tail) tail = fabsf(l[k]);
    printf("peak %.3f tail %.5f\n", peak, tail);
    if (!(peak > 0.01f && peak <= 1.0f) || !(tail < 0.001f)) { puts("FAIL"); return 1; }
    puts("ok");
    return 0;
}
