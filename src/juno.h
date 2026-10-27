/* SPDX-License-Identifier: MPL-2.0 */
/* J6: a six-voice JUNO-60 style synthesizer engine (one DCO per voice, 24 dB resonant low-pass, HPF, one ADSR,
 * LFO, arpeggiator and the stereo chorus). No platform calls: the app owns the UI and the audio callback, and
 * every call below except j_post / j_cmd / the parameter reads runs in the audio interrupt. */
#pragma once
#include <stdint.h>

#define J_VOICES 6
#define J_NPATCH 56                      /* 8 banks of 8, like the hardware's memories */

enum { J_RANGE_16, J_RANGE_8, J_RANGE_4 };
enum { J_MODE_POLY, J_MODE_UNISON, J_MODE_SOLO, J_NMODE };
enum { J_CHO_OFF, J_CHO_I, J_CHO_II, J_CHO_I_II, J_NCHO };
enum { J_ARP_UP, J_ARP_DOWN, J_ARP_UPDN, J_NARP };

/* one sound: 7 fields of 4 bytes after the name; every field 0..127 unless noted */
typedef struct {
    char name[8];                        /* space padded, not terminated */
    uint8_t saw, pulse, sub, noise;      /* DCO mix: levels (the hardware's switches and sliders) */
    uint8_t range;                       /* 0 16', 1 8', 2 4' */
    uint8_t pw;                          /* pulse width (manual) or its LFO depth */
    uint8_t pwm_src;                     /* 0 manual, 1 LFO */
    uint8_t lfo_dco;                     /* LFO to pitch */
    uint8_t lfo_rate, lfo_delay;
    uint8_t lfo_vcf;                     /* LFO to cutoff */
    uint8_t hpf;                         /* 0 bass boost, 1 flat, 2, 3 progressively thinner */
    uint8_t cutoff, res, env_amt, kbd;   /* VCF */
    uint8_t vca_mode;                    /* 0 envelope, 1 gate */
    uint8_t env_pol;                     /* 0 normal, 1 inverted (for the filter) */
    uint8_t a, d, s, r;
    uint8_t chorus;                      /* J_CHO_* */
    uint8_t level;
    uint8_t mode;                        /* J_MODE_* */
    uint8_t porta;
    uint8_t detune;                      /* unison spread */
    uint8_t bend;                        /* pitch bend range, semitones 0..12 */
} j_patch_t;

typedef struct {                         /* not part of a sound */
    int8_t tune;                         /* cents, -50..50 */
    uint8_t vel;                         /* velocity sensitivity 0..127 */
    uint8_t arp_mode, arp_range, arp_rate;   /* J_ARP_*, octaves 1..3, 0..127 */
} j_global_t;

extern j_patch_t j_p;                    /* the live sound: the UI edits it, the engine reads it each block */
extern j_global_t j_g;
extern volatile uint8_t j_arp_on, j_hold;      /* read-only for the UI: set through j_cmd */
extern volatile uint8_t j_nvoices;       /* sounding now */
/* a 16 step recorder: REC clears it and takes each key (J_CMD_REST a rest) up to J_SEQ_MAX, SEQ plays it at the
 * arpeggiator's rate. The UI copies the pattern to and from the settings (j_seq, then j_seq_n: before audio only) */
#define J_SEQ_MAX 16
#define J_SEQ_REST 0xFFu
extern uint8_t j_seq[J_SEQ_MAX], j_seq_vel[J_SEQ_MAX];
extern volatile uint8_t j_seq_n, j_seq_on, j_seq_rec, j_seq_pos;
/* called (audio interrupt) with a MIDI status 0x90 / 0x80, note, velocity for each note the arpeggiator or the
 * sequencer plays; NULL: none. Keys played by hand are not reported: the app sends those itself */
extern void (*j_tx)(uint8_t status, uint8_t note, uint8_t vel);
extern volatile uint32_t j_cc_count;     /* counts the parameter changes that came by MIDI CC */

int j_factory_count(void);               /* the memories a fresh unit comes with */
void j_factory(int i, j_patch_t *out);

void j_init(void);
void j_event(uint8_t status, uint8_t d1, uint8_t d2);   /* a MIDI channel message: the audio interrupt only */
void j_post(uint8_t status, uint8_t d1, uint8_t d2);    /* the same from the main loop (queued) */
enum { J_CMD_ARP, J_CMD_HOLD, J_CMD_PANIC, J_CMD_SEQ, J_CMD_REC, J_CMD_REST };
void j_cmd(int cmd, int val);            /* val 0/1, or -1 to toggle; queued */
void j_render(int32_t *lr, uint32_t n, float gain);    /* audio interrupt: n stereo frames of 24-bit */
void j_silence(void);                    /* every voice off now */
void j_shed(void);                       /* the render ran late: one voice fewer until the sound stops */
void j_default_patch(j_patch_t *p);      /* "INIT" */
