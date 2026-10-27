/* SPDX-License-Identifier: MPL-2.0 */
/* J6 engine: see juno.h. Float, 44.1 kHz, the sound made in chunks of SB samples (the controls move once a chunk:
 * 0.36 ms). One DCO per voice (band-limited saw, pulse and sub square, noise), a 24 dB/oct resonant low-pass solved
 * zero-delay, a one-pole high-pass, one ADSR for filter and amplifier, a triangle LFO, an arpeggiator and the
 * two-line chorus. Built at -O2 on the device (fm1.toml o2). */
#include <stdint.h>
#include <stddef.h>
#include "fastmath.h"
#include "juno.h"

#define SR 44100.0f
#define SB 16
#define QN 64u

typedef char j_patch_size_check[sizeof(j_patch_t) == 36 ? 1 : -1];

j_patch_t j_p;
j_global_t j_g = {0, 40, J_ARP_UP, 1, 80};
volatile uint8_t j_arp_on, j_hold, j_nvoices;
uint8_t j_seq[J_SEQ_MAX], j_seq_vel[J_SEQ_MAX];
volatile uint8_t j_seq_n, j_seq_on, j_seq_rec, j_seq_pos;
volatile uint32_t j_cc_count;             /* parameter changes from MIDI: the screen redraws */
void (*j_tx)(uint8_t, uint8_t, uint8_t);  /* the notes the arpeggiator and the sequencer play, to send on */

/* ------------------------------------------------------------------- state --- */
typedef struct {
    float ph, pitch, env, amp;
    float s1, s2, s3, s4, hp;
    float fine, cut_off;                 /* this voice's own tuning and cutoff error: no two are the same */
    float uni;                           /* unison spread position -1..1 */
    uint32_t rng, age;
    uint8_t note, gate, stage, active, sub_hi, vel;
} voice_t;

static voice_t vo[J_VOICES];
static uint32_t age_ctr, noise_seed = 0x2545F491u;
static uint8_t vmax = J_VOICES, cur_mode;
static float last_pitch = 60.0f;

static float lfo_ph, lfo_t, bend, mw, at_pr, g_now;
static uint8_t sus_on;
static uint8_t down[128], lat[128];
static uint8_t stk[16], nstk;
static uint8_t arp_n[24], arp_cnt, arp_gate, arp_vel;
static int arp_idx, arp_cur = -1;
static float arp_t;
static int sq_idx, sq_cur = -1;
static uint8_t sq_gate;
static float sq_t;

static volatile uint32_t qb[QN], q_w, q_r;

/* per-call constants from the patch */
static struct {
    float ls, lp, lb, ln, cA, cD, cR, sus, cP, level;
    float aH, hpk;
    float dco_d, vcf_d, lfo_inc, lfo_delay;
    float vel_s;
    int range_semi;
    float base_oc, env_oc, kbd, k_res;
    uint8_t vca_gate, pol, hpf, pwm_lfo;
    float pw;
    float unispread;
} bp;

static float cbuf[1024];
static uint32_t cw;
static float cph, clpL, clpR, c_rate, c_cen, c_dep;
static uint8_t c_mode;
static float mixb[SB];

/* -------------------------------------------------------------------- helpers --- */
static inline float tanh_p(float x)      /* a rational tanh: unity gain at zero, soft ceiling at 1 */
{
    if (x > 3.0f)
        return 1.0f;
    if (x < -3.0f)
        return -1.0f;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static inline float blep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

static inline float tri(float ph) { return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; }

static inline float f127(uint8_t v) { return (float)v * (1.0f / 127.0f); }

static uint32_t xs(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

/* ------------------------------------------------------------------ patches --- */
void j_default_patch(j_patch_t *p)
{
    static const uint8_t V[28] = {100, 0, 0, 0, 1, 0, 0, 0, 40, 0, 0, 1, 90, 0, 0, 64, 0, 0, 0, 40, 127, 20, 0, 100, 0, 0, 0, 2};
    uint8_t *d = (uint8_t *)p + 8;
    int i;
    for (i = 0; i < 8; i++)
        p->name[i] = "INIT    "[i];
    for (i = 0; i < 28; i++)
        d[i] = V[i];
}

typedef struct {
    const char *n;
    uint8_t v[28];
} fac_t;

static const fac_t FAC[] = {
    {"BRASS 1 ", {105, 0, 45, 0, 1, 0, 0, 0, 40, 0, 0, 1, 50, 22, 75, 70, 0, 0, 22, 55, 95, 30, 1, 100, 0, 0, 0, 2}},
    {"STRINGS ", {110, 75, 0, 0, 1, 60, 1, 0, 28, 0, 0, 1, 88, 12, 12, 85, 0, 0, 75, 60, 110, 80, 2, 100, 0, 0, 0, 2}},
    {"WARM PAD", {100, 100, 0, 0, 1, 50, 1, 0, 18, 0, 0, 1, 70, 18, 35, 64, 0, 0, 90, 70, 100, 95, 2, 100, 0, 0, 0, 2}},
    {"SYN BASS", {85, 0, 105, 0, 0, 0, 0, 0, 40, 0, 0, 1, 28, 62, 88, 52, 0, 0, 0, 38, 0, 14, 0, 105, 0, 0, 0, 2}},
    {"SUB BASS", {0, 100, 110, 0, 0, 20, 0, 0, 40, 0, 0, 1, 36, 35, 60, 40, 0, 0, 0, 45, 40, 18, 0, 105, 0, 0, 0, 2}},
    {"SWEEP   ", {100, 100, 0, 0, 1, 40, 1, 0, 22, 0, 0, 1, 24, 85, 95, 60, 0, 0, 80, 100, 120, 95, 3, 95, 0, 0, 0, 2}},
    {"LEAD    ", {110, 40, 0, 0, 1, 40, 0, 12, 55, 50, 0, 1, 70, 30, 45, 80, 0, 0, 0, 40, 100, 25, 1, 100, 2, 18, 0, 2}},
    {"SQ LEAD ", {0, 110, 0, 0, 1, 0, 0, 8, 50, 60, 0, 1, 85, 20, 30, 80, 0, 0, 5, 50, 100, 25, 1, 100, 2, 10, 0, 2}},
    {"PLUCK   ", {90, 60, 0, 0, 1, 45, 0, 0, 40, 0, 0, 1, 40, 35, 80, 64, 0, 0, 0, 32, 0, 28, 2, 105, 0, 0, 0, 2}},
    {"ORGAN   ", {0, 100, 100, 0, 1, 0, 0, 0, 45, 0, 0, 1, 105, 0, 0, 64, 1, 0, 0, 0, 127, 5, 2, 95, 0, 0, 0, 2}},
    {"KEYS    ", {90, 70, 0, 0, 1, 0, 0, 0, 40, 0, 0, 1, 62, 20, 55, 70, 0, 0, 0, 45, 40, 40, 1, 100, 0, 0, 0, 2}},
    {"WOBBLE  ", {100, 0, 70, 0, 0, 0, 0, 0, 20, 0, 100, 1, 50, 70, 0, 50, 0, 0, 0, 30, 127, 20, 0, 100, 2, 0, 0, 2}},
    {"WIND    ", {0, 0, 0, 110, 1, 0, 0, 0, 20, 0, 50, 1, 50, 75, 30, 40, 0, 0, 55, 60, 100, 70, 2, 95, 0, 0, 0, 2}},
    {"ZAPPER  ", {80, 0, 0, 100, 1, 0, 0, 0, 40, 0, 0, 2, 110, 55, 100, 0, 0, 1, 0, 40, 0, 30, 0, 100, 0, 0, 0, 2}},
    {"UNISON  ", {100, 0, 70, 0, 1, 0, 0, 0, 40, 0, 0, 1, 72, 22, 45, 64, 0, 0, 8, 50, 100, 30, 1, 95, 1, 0, 60, 2}},
    {"EP BELL ", {70, 50, 0, 0, 2, 35, 1, 0, 40, 0, 0, 1, 80, 10, 40, 90, 0, 0, 0, 60, 0, 60, 2, 100, 0, 0, 0, 2}},
};

int j_factory_count(void) { return (int)(sizeof FAC / sizeof FAC[0]); }

void j_factory(int i, j_patch_t *p)
{
    int k;
    uint8_t *d = (uint8_t *)p + 8;
    if (i < 0 || i >= j_factory_count()) {
        j_default_patch(p);
        return;
    }
    for (k = 0; k < 8; k++)
        p->name[k] = FAC[i].n[k];
    for (k = 0; k < 28; k++)
        d[k] = FAC[i].v[k];
}

/* ------------------------------------------------------------------- voices --- */
static void gate_off_voice(voice_t *v)
{
    if (v->gate) {
        v->gate = 0;
        v->stage = 3;
    }
}

static int gates_held(void)
{
    int i, n = 0;
    for (i = 0; i < J_VOICES; i++)
        n += vo[i].active && vo[i].gate;
    return n;
}

static void start_voice(voice_t *v, int note, int vel, float uni)
{
    if (!gates_held())
        lfo_t = 0.0f;
    if (!v->active) {
        v->env = 0.0f;
        v->amp = 0.0f;
        v->s1 = v->s2 = v->s3 = v->s4 = v->hp = 0.0f;
        v->ph = (float)(xs(&v->rng) & 0xFFFFu) * (1.0f / 65536.0f);
        v->pitch = j_p.porta ? last_pitch : (float)note;
    } else if (!j_p.porta) {
        v->pitch = (float)note;
    }
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->uni = uni;
    v->gate = 1;
    v->stage = 1;
    v->active = 1;
    v->age = ++age_ctr;
    last_pitch = (float)note;
}

static int alloc_voice(int note)
{
    int i, best = -1;
    static int rr;
    for (i = 0; i < vmax; i++)           /* the same note again: its own voice */
        if (vo[i].active && vo[i].note == note)
            return i;
    for (i = 0; i < vmax; i++) {         /* a free one, after the last used */
        int k = (rr + i) % vmax;
        if (!vo[k].active) {
            rr = (k + 1) % vmax;
            return k;
        }
    }
    for (i = 0; i < vmax; i++)           /* the quietest one that is releasing */
        if (!vo[i].gate && (best < 0 || vo[i].env < vo[best].env))
            best = i;
    if (best >= 0)
        return best;
    for (i = 0; i < vmax; i++)           /* the oldest */
        if (best < 0 || vo[i].age < vo[best].age)
            best = i;
    return best;
}

static void stk_remove(int n)
{
    int i, k = 0;
    for (i = 0; i < nstk; i++)
        if (stk[i] != n)
            stk[k++] = stk[i];
    nstk = (uint8_t)k;
}

static void set_group(int retrig, int vel)  /* unison and solo: the voices follow the top of the stack */
{
    int i, cnt = cur_mode == J_MODE_UNISON ? vmax : 1;
    for (i = 0; i < cnt; i++) {
        float u = cnt > 1 ? ((float)i - (float)(cnt - 1) * 0.5f) / ((float)(cnt - 1) * 0.5f) : 0.0f;
        if (retrig)
            start_voice(&vo[i], stk[nstk - 1], vel, u);
        else
            vo[i].note = stk[nstk - 1];
    }
}

static void trig_on(int note, int vel)
{
    if (cur_mode == J_MODE_POLY) {
        start_voice(&vo[alloc_voice(note)], note, vel, 0.0f);
        return;
    }
    stk_remove(note);
    if (nstk == 16u) {
        int i;
        for (i = 1; i < 16; i++) {       /* rotate the oldest to the end by swaps (a plain shift compiles to memmove) */
            uint8_t t = stk[i - 1];
            stk[i - 1] = stk[i];
            stk[i] = t;
        }
        nstk--;
    }
    stk[nstk++] = (uint8_t)note;
    set_group(1, vel);
}

static void trig_off(int note)
{
    int i;
    if (cur_mode == J_MODE_POLY) {
        for (i = 0; i < J_VOICES; i++)
            if (vo[i].active && vo[i].gate && vo[i].note == note)
                gate_off_voice(&vo[i]);
        return;
    }
    stk_remove(note);
    if (nstk) {
        set_group(0, 0);
    } else {
        for (i = 0; i < J_VOICES; i++)
            gate_off_voice(&vo[i]);
    }
}

/* notes made by the arpeggiator or the sequencer: sounded here and reported through j_tx */
static void gon(int note, int vel)
{
    if (j_tx)
        j_tx(0x90u, (uint8_t)note, (uint8_t)vel);
    trig_on(note, vel);
}

static void goff(int note)
{
    if (j_tx)
        j_tx(0x80u, (uint8_t)note, 0);
    trig_off(note);
}

static void all_off(void)
{
    int i;
    if (arp_cur >= 0 && j_tx)
        j_tx(0x80u, (uint8_t)arp_cur, 0);
    if (sq_cur >= 0 && j_tx)
        j_tx(0x80u, (uint8_t)sq_cur, 0);
    for (i = 0; i < J_VOICES; i++)
        gate_off_voice(&vo[i]);
    nstk = 0;
    arp_gate = 0;
    arp_cur = -1;
    sq_gate = 0;
    sq_cur = -1;
}

/* ---------------------------------------------------------------------- arp --- */
static int clamp_range(int r) { return r < 1 ? 1 : r > 3 ? 3 : r; }

static void arp_add(int n, int vel)
{
    int i;
    arp_vel = (uint8_t)vel;
    for (i = 0; i < arp_cnt; i++)
        if (arp_n[i] == n)
            return;
    if (arp_cnt == 24u)
        return;
    arp_n[arp_cnt] = (uint8_t)n;         /* sorted insert by swaps (a plain shift compiles to memmove) */
    for (i = arp_cnt; i > 0 && arp_n[i - 1] > arp_n[i]; i--) {
        uint8_t t = arp_n[i - 1];
        arp_n[i - 1] = arp_n[i];
        arp_n[i] = t;
    }
    arp_cnt++;
    if (arp_cnt == 1u) {
        arp_idx = 0;
        arp_t = 1e9f;                    /* the first step now */
    }
}

static void arp_del(int n)
{
    int i, k = 0;
    for (i = 0; i < arp_cnt; i++)
        if (arp_n[i] != n)
            arp_n[k++] = arp_n[i];
    arp_cnt = (uint8_t)k;
    if (!arp_cnt && arp_cur >= 0) {
        goff(arp_cur);
        arp_cur = -1;
        arp_gate = 0;
    }
}

static float arp_step_len(void) { return SR / (1.5f * fm_exp2f(f127(j_g.arp_rate) * 3.5f)); }

static void arp_step(void)
{
    int range = clamp_range(j_g.arp_range), n = arp_cnt * range, i, p, note;
    if (!arp_cnt)
        return;
    if (arp_cur >= 0)
        goff(arp_cur);
    if (j_g.arp_mode == J_ARP_DOWN) {
        i = n - 1 - arp_idx % n;
    } else if (j_g.arp_mode == J_ARP_UPDN && n > 1) {
        int period = 2 * n - 2;
        p = arp_idx % period;
        i = p < n ? p : period - p;
    } else {
        i = arp_idx % n;
    }
    arp_idx++;
    if (arp_idx > 100000)
        arp_idx = 0;
    note = arp_n[i % arp_cnt] + 12 * (i / arp_cnt);
    if (note > 127)
        note = 127;
    gon(note, arp_vel);
    arp_cur = note;
    arp_gate = 1;
}

/* --------------------------------------------------------------- key events --- */
static uint8_t hold_btn, pedal;

static void eff_on(int n, int vel)
{
    if (j_arp_on)
        arp_add(n, vel);
    else
        trig_on(n, vel);
}

static void eff_off(int n)
{
    if (j_arp_on)
        arp_del(n);
    else
        trig_off(n);
}

static void set_arp(int on);

static void seq_stop(void)
{
    if (sq_cur >= 0)
        goff(sq_cur);
    sq_cur = -1;
    sq_gate = 0;
    j_seq_on = 0;
}

static void seq_rec(int on)
{
    if (on == j_seq_rec)
        return;
    if (on) {
        seq_stop();
        j_seq_n = 0;
    }
    j_seq_rec = (uint8_t)on;
}

static void seq_add(int note, int vel)
{
    if (!j_seq_rec || j_seq_n >= J_SEQ_MAX)
        return;
    j_seq[j_seq_n] = (uint8_t)note;
    j_seq_vel[j_seq_n] = (uint8_t)vel;
    j_seq_n++;
    if (j_seq_n >= J_SEQ_MAX)
        j_seq_rec = 0;
}

static void seq_play(int on)
{
    if (!on) {
        seq_stop();
        return;
    }
    j_seq_rec = 0;
    if (!j_seq_n)
        return;
    if (j_arp_on)
        set_arp(0);
    sq_idx = 0;
    sq_t = 1e9f;                         /* the first step now */
    j_seq_on = 1;
}

static void key_on(int n, int vel)
{
    seq_add(n, vel);
    down[n] = 1;
    lat[n] = 0;
    eff_on(n, vel);
}

static void key_off(int n)
{
    if (!down[n])
        return;
    down[n] = 0;
    if (sus_on)
        lat[n] = 1;
    else
        eff_off(n);
}

static void sustain_update(void)
{
    uint8_t on = (uint8_t)(hold_btn | pedal);
    int n;
    if (!on && sus_on)
        for (n = 0; n < 128; n++)
            if (lat[n]) {
                lat[n] = 0;
                if (!down[n])
                    eff_off(n);
            }
    sus_on = on;
    j_hold = on;
}

static void clear_keys(void)
{
    int n;
    for (n = 0; n < 128; n++)
        down[n] = lat[n] = 0;
    arp_cnt = 0;
    all_off();
}

static void set_arp(int on)
{
    int n;
    if (on == j_arp_on)
        return;
    all_off();
    arp_cnt = 0;
    j_arp_on = (uint8_t)on;
    arp_vel = 100;
    for (n = 0; n < 128; n++)
        if (down[n] || lat[n]) {
            if (on)
                arp_add(n, 100);
            else
                trig_on(n, 100);
        }
}

static const struct { uint8_t cc, off; } CCMAP[] = {
    {5, offsetof(j_patch_t, porta)},    {70, offsetof(j_patch_t, s)},       {71, offsetof(j_patch_t, res)},
    {72, offsetof(j_patch_t, r)},       {73, offsetof(j_patch_t, a)},       {74, offsetof(j_patch_t, cutoff)},
    {75, offsetof(j_patch_t, d)},       {76, offsetof(j_patch_t, lfo_rate)}, {77, offsetof(j_patch_t, lfo_delay)},
    {78, offsetof(j_patch_t, lfo_dco)}, {79, offsetof(j_patch_t, lfo_vcf)}, {7, offsetof(j_patch_t, level)},
};

static void cc(int n, int v)
{
    unsigned i;
    if (n == 1) {
        mw = f127((uint8_t)v);
    } else if (n == 64) {
        pedal = v >= 64;
        sustain_update();
    } else if (n == 120 || n == 123) {
        clear_keys();
    } else if (n == 121) {
        mw = at_pr = bend = 0.0f;
        pedal = 0;
        sustain_update();
    } else {
        for (i = 0; i < sizeof CCMAP / sizeof CCMAP[0]; i++)
            if (CCMAP[i].cc == n) {
                ((uint8_t *)&j_p)[CCMAP[i].off] = (uint8_t)v;
                j_cc_count++;
            }
    }
}

void j_event(uint8_t st, uint8_t d1, uint8_t d2)
{
    d1 &= 0x7Fu;
    switch (st & 0xF0u) {
    case 0x90:
        if (d2 & 0x7Fu)
            key_on(d1, d2 & 0x7F);
        else
            key_off(d1);
        break;
    case 0x80:
        key_off(d1);
        break;
    case 0xB0:
        cc(d1, d2 & 0x7F);
        break;
    case 0xD0:
        at_pr = f127(d1);
        break;
    case 0xE0:
        bend = (float)(((int)(d2 & 0x7Fu) << 7 | d1) - 8192) * (1.0f / 8192.0f);
        break;
    default:
        break;
    }
}

static void command(int c, int v)
{
    if (c == J_CMD_ARP) {
        if (!j_seq_on)
            set_arp(v == 2 ? !j_arp_on : v != 0);
    } else if (c == J_CMD_HOLD) {
        hold_btn = (uint8_t)(v == 2 ? !hold_btn : v != 0);
        sustain_update();
    } else if (c == J_CMD_PANIC) {
        hold_btn = pedal = 0;
        sustain_update();
        seq_rec(0);
        seq_stop();
        clear_keys();
    } else if (c == J_CMD_SEQ) {
        seq_play(v == 2 ? !j_seq_on : v != 0);
    } else if (c == J_CMD_REC) {
        seq_rec(v == 2 ? !j_seq_rec : v != 0);
    } else if (c == J_CMD_REST) {
        seq_add(J_SEQ_REST, 0);
    }
}

void j_post(uint8_t st, uint8_t d1, uint8_t d2)
{
    uint32_t w = q_w;
    if (((w + 1u) % QN) == q_r)
        return;
    qb[w] = (uint32_t)st | (uint32_t)d1 << 8 | (uint32_t)d2 << 16;
    q_w = (w + 1u) % QN;
}

void j_cmd(int c, int v) { j_post(0xF1u, (uint8_t)c, (uint8_t)(v < 0 ? 2 : v)); }

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t e = qb[q_r];
        q_r = (q_r + 1u) % QN;
        if ((e & 0xFFu) == 0xF1u)
            command((int)((e >> 8) & 0xFFu), (int)((e >> 16) & 0xFFu));
        else
            j_event((uint8_t)e, (uint8_t)(e >> 8), (uint8_t)(e >> 16));
    }
}

void j_silence(void)
{
    int i;
    for (i = 0; i < J_VOICES; i++) {
        vo[i].active = vo[i].gate = vo[i].stage = 0;
        vo[i].env = vo[i].amp = 0.0f;
    }
}

void j_shed(void)
{
    int i;
    if (vmax > 3u)
        vmax--;
    for (i = vmax; i < J_VOICES; i++)
        gate_off_voice(&vo[i]);
}

/* ----------------------------------------------------------------- the sound --- */
static void sync_mode(void)
{
    if (j_p.mode != cur_mode && j_p.mode < J_NMODE) {
        all_off();
        cur_mode = j_p.mode;
    }
}

static void prep(void)
{
    const j_patch_t *p = &j_p;
    float t;
    bp.ls = f127(p->saw);
    bp.lp = f127(p->pulse);
    bp.lb = f127(p->sub);
    bp.ln = f127(p->noise) * 0.6f;
    t = 0.001f * fm_exp2f(f127(p->a) * 11.55f);
    bp.cA = 1.0f - fm_expf(-1.609f * SB / (t * SR));
    t = 0.002f * fm_exp2f(f127(p->d) * 12.55f);
    bp.cD = 1.0f - fm_expf(-4.6f * SB / (t * SR));
    t = 0.002f * fm_exp2f(f127(p->r) * 12.55f);
    bp.cR = 1.0f - fm_expf(-4.6f * SB / (t * SR));
    bp.sus = f127(p->s);
    if (p->porta) {
        t = 0.01f * fm_exp2f(f127(p->porta) * 8.2f);
        bp.cP = 1.0f - fm_expf(-(float)SB / (t * SR));
    } else {
        bp.cP = 1.0f;
    }
    bp.level = f127(p->level);
    bp.hpf = p->hpf;
    t = p->hpf == 0 ? 100.0f : p->hpf == 2 ? 240.0f : 720.0f;
    bp.aH = 1.0f - fm_expf(-FM_TWO_PI * t / SR);
    bp.dco_d = f127(p->lfo_dco) * 2.0f;
    bp.vcf_d = f127(p->lfo_vcf) * 2.5f;
    bp.lfo_inc = 0.3f * fm_exp2f(f127(p->lfo_rate) * 6.1f) * (float)SB / SR;
    bp.lfo_delay = f127(p->lfo_delay) * 3.0f;
    bp.vel_s = f127(j_g.vel) * 0.85f;
    bp.base_oc = f127(p->cutoff) * 10.0f;
    bp.env_oc = f127(p->env_amt) * 9.0f;
    bp.kbd = f127(p->kbd);
    bp.k_res = 4.05f * f127(p->res);
    bp.vca_gate = p->vca_mode != 0;
    bp.pol = p->env_pol != 0;
    bp.pwm_lfo = p->pwm_src != 0;
    bp.pw = f127(p->pw);
    bp.range_semi = 12 * ((int)(p->range > 2 ? 1 : p->range) - 1);
    bp.unispread = f127(p->detune) * 0.25f;
    c_mode = p->chorus < J_NCHO ? p->chorus : 0;
    if (c_mode == J_CHO_I) {
        c_rate = 0.513f;
        c_cen = 3.6f;
        c_dep = 1.5f;
    } else if (c_mode == J_CHO_II) {
        c_rate = 0.863f;
        c_cen = 3.6f;
        c_dep = 1.9f;
    } else {
        c_rate = 9.75f;
        c_cen = 3.2f;
        c_dep = 0.25f;
    }
}

static void run_voice(voice_t *v, uint32_t m, float lfo, float lfo_amp, float w)
{
    float semis, inc, freq, oc, fc, g, G, G2, G3, G4, Gc, k, den, a, da, a1, velamp, md;
    float s1 = v->s1, s2 = v->s2, s3 = v->s3, s4 = v->s4, hp = v->hp, ph = v->ph;
    float ls = bp.ls, lp = bp.lp, lb = bp.lb, ln = bp.ln, aH = bp.aH;
    float dc = 2.0f * w - 1.0f;
    uint8_t hi = v->sub_hi, mode_hp = bp.hpf;
    uint32_t i, rng = v->rng;
    const j_patch_t *p = &j_p;

    switch (v->stage) {                  /* the envelope */
    case 1:
        v->env += (1.25f - v->env) * bp.cA;
        if (v->env >= 1.0f) {
            v->env = 1.0f;
            v->stage = 2;
        }
        break;
    case 2:
        v->env += (bp.sus - v->env) * bp.cD;
        break;
    case 3:
        v->env += (0.0f - v->env) * bp.cR;
        if (v->env < 0.0003f) {
            v->env = 0.0f;
            v->stage = 0;
        }
        break;
    default:
        break;
    }
    if (bp.vca_gate)
        a1 = v->amp + ((v->gate ? 1.0f : 0.0f) - v->amp) * 0.3f;
    else
        a1 = v->env;
    a = v->amp;
    da = (a1 - a) * (1.0f / (float)m);
    v->amp = a1;

    v->pitch += ((float)v->note - v->pitch) * bp.cP;
    md = mw > at_pr ? mw : at_pr;
    semis = v->pitch + (float)bp.range_semi + bend * (float)p->bend + lfo * (lfo_amp * bp.dco_d + md) +
            (float)j_g.tune * 0.01f + v->fine + v->uni * bp.unispread;
    freq = 440.0f * fm_exp2f((semis - 69.0f) * (1.0f / 12.0f));
    inc = freq * (1.0f / SR);
    if (inc > 0.45f)
        inc = 0.45f;

    oc = bp.base_oc + bp.kbd * ((v->pitch - 60.0f) * (1.0f / 12.0f)) +
         bp.env_oc * (bp.pol ? -v->env : v->env) + lfo * lfo_amp * bp.vcf_d + v->cut_off;
    fc = 20.0f * fm_exp2f(oc);
    if (fc > 19800.0f)
        fc = 19800.0f;
    g = fm_tanf(FM_PI * fc * (1.0f / SR));
    G = g / (1.0f + g);
    G2 = G * G;
    G3 = G2 * G;
    G4 = G2 * G2;
    Gc = 1.0f - G;
    k = bp.k_res;
    den = 1.0f / (1.0f + k * G4);
    velamp = 1.0f - bp.vel_s * (1.0f - (float)v->vel * (1.0f / 127.0f));
    velamp *= bp.level * 0.42f;

    for (i = 0; i < m; i++) {
        float x = 0.0f, sg, u, v1, y;
        ph += inc;
        if (ph >= 1.0f) {
            ph -= 1.0f;
            hi ^= 1u;
        }
        if (ls > 0.0f)
            x += ls * (2.0f * ph - 1.0f - blep(ph, inc));
        if (lp > 0.0f) {
            float t2 = ph - w;
            if (t2 < 0.0f)
                t2 += 1.0f;
            x += lp * ((ph < w ? 1.0f : -1.0f) + blep(ph, inc) - blep(t2, inc) - dc);
        }
        if (lb > 0.0f) {
            float b = hi ? 1.0f : -1.0f, e = 0.0f;
            if (ph < inc)
                e = (hi ? 1.0f : -1.0f) * blep(ph, inc);
            else if (ph > 1.0f - inc)
                e = (hi ? -1.0f : 1.0f) * blep(ph, inc);
            x += lb * (b + e);
        }
        if (ln > 0.0f) {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            x += ln * (float)(int32_t)rng * 4.6566129e-10f;
        }
        x *= 0.5f;
        sg = Gc * (G3 * s1 + G2 * s2 + G * s3 + s4);
        u = tanh_p((x - k * sg) * den);
        v1 = (u - s1) * G;
        y = v1 + s1;
        s1 = y + v1;
        v1 = (y - s2) * G;
        u = v1 + s2;
        s2 = u + v1;
        v1 = (u - s3) * G;
        y = v1 + s3;
        s3 = y + v1;
        v1 = (y - s4) * G;
        u = v1 + s4;
        s4 = u + v1;
        y = u * (1.0f + 0.2f * k);       /* the passband sags with resonance: partly given back */
        if (mode_hp == 1u) {
        } else if (mode_hp == 0u) {
            hp += aH * (y - hp);
            y += 0.6f * hp;
        } else {
            hp += aH * (y - hp);
            y -= hp;
        }
        mixb[i] += y * a * velamp;
        a += da;
    }
    v->s1 = fm_flush(s1);
    v->s2 = fm_flush(s2);
    v->s3 = fm_flush(s3);
    v->s4 = fm_flush(s4);
    v->hp = fm_flush(hp);
    v->ph = ph;
    v->sub_hi = hi;
    v->rng = rng;
    if (!v->gate && ((bp.vca_gate && v->amp < 0.0005f) || (!bp.vca_gate && v->stage == 0))) {
        v->active = 0;
        v->stage = 0;
    }
}

static inline float tap(float dl)
{
    int di = (int)dl;
    float fr = dl - (float)di;
    float a = cbuf[(cw - (uint32_t)di) & 1023u], b = cbuf[(cw - (uint32_t)di - 1u) & 1023u];
    return a + (b - a) * fr;
}

static void chunk(uint32_t m, int32_t *out, float gain)
{
    float lfo, lfo_amp, w, len;
    uint32_t i;
    int n;

    lfo_ph += bp.lfo_inc;
    if (lfo_ph >= 1.0f)
        lfo_ph -= 1.0f;
    lfo_t += (float)SB / SR;
    lfo = tri(lfo_ph);
    lfo_amp = bp.lfo_delay > 0.0f ? (lfo_t >= bp.lfo_delay ? 1.0f : lfo_t / bp.lfo_delay) : 1.0f;
    w = 0.5f + 0.44f * bp.pw * (bp.pwm_lfo ? 0.5f + 0.5f * lfo : 1.0f);

    if (j_arp_on) {
        arp_t += (float)m;
        len = arp_step_len();
        if (arp_t >= len) {
            arp_t = arp_t > len * 4.0f ? 0.0f : arp_t - len;
            arp_step();
        } else if (arp_gate && arp_t >= len * 0.5f) {
            if (arp_cur >= 0)
                goff(arp_cur);
            arp_cur = -1;
            arp_gate = 0;
        }
    }

    if (j_seq_on) {
        sq_t += (float)m;
        len = arp_step_len();
        if (sq_t >= len) {
            sq_t = sq_t > len * 4.0f ? 0.0f : sq_t - len;
            if (sq_cur >= 0)
                goff(sq_cur);
            sq_cur = -1;
            if (sq_idx >= j_seq_n)
                sq_idx = 0;
            j_seq_pos = (uint8_t)sq_idx;
            if (j_seq[sq_idx] != J_SEQ_REST) {
                sq_cur = j_seq[sq_idx];
                gon(sq_cur, j_seq_vel[sq_idx]);
                sq_gate = 1;
            }
            sq_idx++;
        } else if (sq_gate && sq_t >= len * 0.5f) {
            if (sq_cur >= 0)
                goff(sq_cur);
            sq_cur = -1;
            sq_gate = 0;
        }
    }

    for (i = 0; i < m; i++)
        mixb[i] = 0.0f;
    for (n = 0; n < J_VOICES; n++)
        if (vo[n].active)
            run_voice(&vo[n], m, lfo, lfo_amp, w);

    for (i = 0; i < m; i++) {
        float d = mixb[i], l = d, r = d;
        cbuf[cw & 1023u] = d;
        if (c_mode) {
            float t, wl, wr;
            cph += c_rate * (1.0f / SR);
            if (cph >= 1.0f)
                cph -= 1.0f;
            t = tri(cph) * c_dep;
            wl = tap((c_cen + t) * (SR * 0.001f));
            wr = tap((c_cen - t) * (SR * 0.001f));
            clpL += 0.6f * (wl - clpL);      /* the delay line's dulled top */
            clpR += 0.6f * (wr - clpR);
            l = (d + clpL) * 0.7f;
            r = (d + clpR) * 0.7f;
        }
        cw++;
        g_now += (gain - g_now) * 0.004f;
        out[2 * i] = (int32_t)(tanh_p(l * 1.8f) * g_now);
        out[2 * i + 1] = (int32_t)(tanh_p(r * 1.8f) * g_now);
    }
}

void j_render(int32_t *lr, uint32_t n, float gain)
{
    int i, act = 0;
    sync_mode();
    drain();
    prep();
    while (n) {
        uint32_t m = n < SB ? n : SB;
        chunk(m, lr, gain);
        lr += 2u * m;
        n -= m;
    }
    for (i = 0; i < J_VOICES; i++)
        act += vo[i].active;
    j_nvoices = (uint8_t)act;
    if (!act)
        vmax = J_VOICES;
}

void j_init(void)
{
    int i;
    uint32_t s = noise_seed;
    for (i = 0; i < J_VOICES; i++) {
        vo[i].rng = xs(&s) | 1u;
        vo[i].fine = ((float)(int)(xs(&s) % 200u) - 100.0f) * 0.00015f;
        vo[i].cut_off = ((float)(int)(xs(&s) % 200u) - 100.0f) * 0.0003f;
        vo[i].active = 0;
    }
    j_default_patch(&j_p);
    cur_mode = 0;
    vmax = J_VOICES;
}
