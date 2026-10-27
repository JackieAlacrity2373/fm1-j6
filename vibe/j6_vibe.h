/* SPDX-License-Identifier: MPL-2.0 */
/* J6 for VIBE FM: the platform-neutral wrapper around the JUNO engine (src/juno.c). Prepared before the VIBE FM SDK
 * is public: it has no SDK calls. The SDK glue (vibe/README.md) maps the host's MIDI, audio block and parameters onto
 * these four functions. The engine renders at a fixed 44100 Hz. */
#pragma once
#include <stdint.h>

#define J6V_RATE 44100

void j6v_init(void);
void j6v_midi(uint8_t status, uint8_t d1, uint8_t d2);       /* note on/off, CC, pitch bend, program change */
void j6v_program(int n);                                     /* factory memory 0..j_factory_count()-1 */
void j6v_process(float *left, float *right, uint32_t n);     /* n frames, -1..1, any n (blocks of up to 256) */
